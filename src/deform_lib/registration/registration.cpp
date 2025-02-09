#include <deform_lib/arg_parser.h>
#include <deform_lib/config.h>
#include <deform_lib/defer.h>
#include <deform_lib/filters/resample.h>
#include <deform_lib/jacobian.h>
#include <deform_lib/registration/registration_engine.h>
#include <deform_lib/registration/settings.h>
#include <deform_lib/registration/transform.h>
#include <deform_lib/registration/volume_pyramid.h>

#ifdef DF_USE_CUDA
    #include <deform_lib/registration/gpu_registration_engine.h>
#endif


#include <stk/common/assert.h>
#include <stk/common/log.h>
#include <stk/filters/normalize.h>
#include <stk/image/volume.h>
#include <stk/io/io.h>
#include <stk/math/math.h>
#include <stk/math/matrix3x3f.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <omp.h>
#include <optional>
#include <string>
#include <vector>

#include "registration.h"

/// name : Name for printout
void validate_volume_properties(
    const stk::Volume& vol,
    const stk::Volume& ref_vol,
    const std::string& name)
{
    std::ostringstream osstr;

    // Validate size
    if (vol.size() != ref_vol.size())
    {
        osstr << "Dimension mismatch for " << name
              << " (size: " << vol.size() << ", expected: "
              << ref_vol.size() << ")";
        throw ValidationError(osstr.str());
    }

    // Validate origin
    if (stk::nonzero(vol.origin() - ref_vol.origin()))
    {
        osstr << "Origin mismatch for " << name
              << " (origin: " << vol.origin() << ", expected: "
              << ref_vol.origin() << ")";
        throw ValidationError(osstr.str());
    }

    // Validate spacing
    if (stk::nonzero(vol.spacing() - ref_vol.spacing()))
    {
        osstr << "Spacing mismatch for " << name
              << " (spacing: " << vol.spacing() << ", expected: "
              << ref_vol.spacing() << ")";
        throw ValidationError(osstr.str());
    }

    // Validate direction
    if (stk::nonzero(vol.direction() - ref_vol.direction()))
    {
        osstr << "Direction mismatch for " << name
              << " (direction: " << vol.direction() << ", expected: "
              << ref_vol.direction() << ")";
        throw ValidationError(osstr.str());
    }
}

template<typename TEngine>
stk::Volume registration(
        const Settings& settings,
        std::vector<stk::Volume>& fixed_volumes,
        std::vector<stk::Volume>& moving_volumes,
        const std::optional<stk::Volume> fixed_mask,
        const std::optional<stk::Volume> moving_mask,
        const std::optional<std::vector<float3>> fixed_landmarks,
        const std::optional<std::vector<float3>> moving_landmarks,
        const std::optional<stk::Volume> initial_deformation,
        const std::optional<stk::Volume> constraint_mask,
        const std::optional<stk::Volume> constraint_values,
        const int num_threads
)
{
    ASSERT(fixed_volumes.size() == moving_volumes.size());

    LOG(Info) << "Running registration";

    if (num_threads > 0) {
        LOG(Info) << "Number of threads: " << num_threads;
        omp_set_num_threads(num_threads);
    }

    TEngine engine(settings);

    // Rules:
    // * All volumes for the same subject (i.e. fixed or moving) must have the
    //      same dimensions.
    // * All volumes for the same subject (i.e. fixed or moving) need to have
    //      the same origin and spacing.
    // * For simplicity any given initial deformation field must match the
    //      fixed image properties (size, origin, spacing).
    // * Pairs must have a matching data type
    // If hard constraints are enabled:
    // * Constraint mask and values must match fixed image

    stk::Volume fixed_ref; // Reference volume for validation
    stk::Volume moving_ref; // Reference volume for computing the jacobian

    // Input images
    for (size_t i = 0; i < fixed_volumes.size(); ++i) {
        std::string fixed_id = "fixed" + std::to_string(i);
        std::string moving_id = "moving" + std::to_string(i);

        stk::Volume& fixed = fixed_volumes[i];
        if (!fixed.valid()) {
            throw ValidationError("Invalid fixed volume at index " + std::to_string(i));
        }
        stk::Volume& moving = moving_volumes[i];
        if (!moving.valid()) {
            throw ValidationError("Invalid moving volume at index " + std::to_string(i));
        }

        if (fixed.voxel_type() != moving.voxel_type()) {
            throw ValidationError("Mismatch in voxel type between pairs at index "
                                  + std::to_string(i) + ", "
                                  + "fixed type '" + stk::as_string(fixed.voxel_type()) + "', "
                                  + "moving type '" + stk::as_string(moving.voxel_type()) + "'.");
        }

        if (!fixed_ref.valid() || !moving_ref.valid()) {
            fixed_ref = fixed;
            moving_ref = moving;
        }
        else {
            validate_volume_properties(fixed, fixed_ref, fixed_id);
            validate_volume_properties(moving, moving_ref, moving_id);
        }

        auto& slot = settings.image_slots[i];
        if (slot.normalize) {
            if (fixed.voxel_type() == stk::Type_Float &&
                moving.voxel_type() == stk::Type_Float) {
                fixed = stk::normalize<float>(fixed, 0.0f, 1.0f);
                moving = stk::normalize<float>(moving, 0.0f, 1.0f);
            }
            else if (fixed.voxel_type() == stk::Type_Double &&
                     moving.voxel_type() == stk::Type_Double) {
                fixed = stk::normalize<double>(fixed, 0.0, 1.0);
                moving = stk::normalize<double>(moving, 0.0, 1.0);
            }
            else {
                throw ValidationError("Normalize only supported on volumes of type float or double");
            }
        }


        engine.set_image_pair(static_cast<int>(i), fixed, moving);
    }

    // Fixed mask
    if (fixed_mask.has_value()) {
        if (!fixed_mask.value().valid()) {
            throw ValidationError("Invalid fixed mask");
        }
        validate_volume_properties(fixed_mask.value(), fixed_ref, "fixed mask");
        engine.set_fixed_mask(fixed_mask.value());
    }

    // Moving mask
    if (moving_mask.has_value()) {
        if (!moving_mask.value().valid()) {
            throw ValidationError("Invalid moving mask");
        }
        validate_volume_properties(moving_mask.value(), moving_ref, "moving mask");
        engine.set_moving_mask(moving_mask.value());
    }

    // Initial deformation
    if (initial_deformation.has_value()) {
        if (!initial_deformation.value().valid()) {
            throw ValidationError("Invalid initial deformation volume");
        }

        validate_volume_properties(initial_deformation.value(), fixed_ref, "initial deformation field");
        engine.set_initial_deformation(initial_deformation.value());
    }

    // Constraints
    if (constraint_mask.has_value() && constraint_values.has_value()) {
        if (!constraint_mask.value().valid()) {
            throw ValidationError("Invalid constraint mask volume");
        }

        if (!constraint_values.value().valid()) {
            throw ValidationError("Invalid constraint values volume");
        }

        validate_volume_properties(constraint_mask.value(), fixed_ref, "constraint mask");
        validate_volume_properties(constraint_values.value(), fixed_ref, "constraint values");
        engine.set_voxel_constraints(constraint_mask.value(), constraint_values.value());
    }

    // Landmarks
    if (fixed_landmarks.has_value() || moving_landmarks.has_value()) {
        if (!fixed_landmarks.has_value() || !moving_landmarks.has_value()) {
            throw ValidationError("Landmarks must be specified for both fixed and moving");
        }

        if (fixed_landmarks.value().size() != moving_landmarks.value().size()) {
            throw ValidationError("The number of fixed and moving landmarks must match");
        }

        engine.set_landmarks(fixed_landmarks.value(), moving_landmarks.value());
    }

    using namespace std::chrono;
    auto t_start = high_resolution_clock::now();
    stk::Volume def = engine.execute();
    auto t_end = high_resolution_clock::now();
    int elapsed = int(round(duration_cast<duration<double>>(t_end - t_start).count()));
    LOG(Info) << "Registration completed in "
              << elapsed / 60
              << ":"
              << std::right << std::setw(2) << std::setfill('0') << elapsed % 60;

    return def;
}

stk::Volume registration(
        const Settings& settings,
        std::vector<stk::Volume>& fixed_volumes,
        std::vector<stk::Volume>& moving_volumes,
        const std::optional<stk::Volume> fixed_mask,
        const std::optional<stk::Volume> moving_mask,
        const std::optional<std::vector<float3>> fixed_landmarks,
        const std::optional<std::vector<float3>> moving_landmarks,
        const std::optional<stk::Volume> initial_deformation,
        const std::optional<stk::Volume> constraint_mask,
        const std::optional<stk::Volume> constraint_values,
        const int num_threads
#ifdef DF_USE_CUDA
        , bool use_gpu
#endif
        )
{
#ifdef DF_USE_CUDA
    // TODO: Check cuda availability
    if (use_gpu) {
        return registration<GpuRegistrationEngine>(
            settings,
            fixed_volumes,
            moving_volumes,
            fixed_mask,
            moving_mask,
            fixed_landmarks,
            moving_landmarks,
            initial_deformation,
            constraint_mask,
            constraint_values,
            num_threads
        );
    }else{
#endif
        return registration<RegistrationEngine>(
            settings,
            fixed_volumes,
            moving_volumes,
            fixed_mask,
            moving_mask,
            fixed_landmarks,
            moving_landmarks,
            initial_deformation,
            constraint_mask,
            constraint_values,
            num_threads
        );
#ifdef DF_USE_CUDA
    }
#endif
}
