#include "transform.h"

#include <stk/common/assert.h>
#include <stk/cuda/cuda.h>
#include <stk/cuda/volume.h>
#include <stk/image/gpu_volume.h>
#include <stk/math/float3.h>
#include <stk/math/float4.h>

namespace cuda = stk::cuda;

template<typename T>
__global__ void transform_kernel_linear(
    cuda::VolumePtr<T> src,
    dim3 src_dims,
    cuda::VolumePtr<float4> df,
    dim3 df_dims,
    float3 fixed_origin,
    float3 fixed_spacing,
    float3 moving_origin,
    float3 inv_moving_spacing,
    cuda::VolumePtr<T> out
)
{
    int x = blockIdx.x*blockDim.x + threadIdx.x;
    int y = blockIdx.y*blockDim.y + threadIdx.y;
    int z = blockIdx.z*blockDim.z + threadIdx.z;

    if (x >= df_dims.x ||
        y >= df_dims.y ||
        z >= df_dims.z)
    {
        return;
    }

    float3 wp = float3{float(x), float(y), float(z)} * fixed_spacing
                + fixed_origin;

    float4 d = df(x,y,z);
    float3 mp = (wp + float3{d.x, d.y, d.z} - moving_origin)
                * inv_moving_spacing;

    out(x,y,z) = cuda::linear_at_border(src, src_dims, mp.x, mp.y, mp.z);
}

template<typename T>
__global__ void transform_kernel_nn(
    cuda::VolumePtr<T> src,
    dim3 src_dims,
    cuda::VolumePtr<float4> df,
    dim3 df_dims,
    float3 fixed_origin,
    float3 fixed_spacing,
    float3 moving_origin,
    float3 inv_moving_spacing,
    cuda::VolumePtr<T> out
)
{
    int x = blockIdx.x*blockDim.x + threadIdx.x;
    int y = blockIdx.y*blockDim.y + threadIdx.y;
    int z = blockIdx.z*blockDim.z + threadIdx.z;

    if (x >= df_dims.x ||
        y >= df_dims.y ||
        z >= df_dims.z)
    {
        return;
    }

    float3 wp = float3{float(x), float(y), float(z)} * fixed_spacing
                + fixed_origin;

    float4 d = df(x,y,z);
    float3 mp = (wp + float3{d.x, d.y, d.z} - moving_origin)
                * inv_moving_spacing;
    
    int xt = roundf(mp.x);
    int yt = roundf(mp.y);
    int zt = roundf(mp.z);

    if (xt >= 0 && xt < src_dims.x &&
        yt >= 0 && yt < src_dims.y &&
        zt >= 0 && zt < src_dims.z) {
        
        out(x,y,z) = src(xt, yt, zt);
    }
    else {
        out(x,y,z) = T{0};
    }
}

static void run_nn_kernel(
    stk::Type type,
    const dim3& grid_size,
    const dim3& block_size,
    const stk::GpuVolume& src,
    const stk::GpuVolume& df,
    stk::GpuVolume& out
)
{
    float3 inv_moving_spacing = {
        1.0f / src.spacing().x,
        1.0f / src.spacing().y,
        1.0f / src.spacing().z,
    };

    switch (type) {
    case stk::Type_Char:
        transform_kernel_nn<char><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Char2:
        transform_kernel_nn<char2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Char4:
        transform_kernel_nn<char4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;

    case stk::Type_UChar:
        transform_kernel_nn<uint8_t><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_UChar2:
        transform_kernel_nn<uchar2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_UChar4:
        transform_kernel_nn<uchar4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;

    case stk::Type_Short:
        transform_kernel_nn<short><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Short2:
        transform_kernel_nn<short2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Short4:
        transform_kernel_nn<short4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;

    case stk::Type_UShort:
        transform_kernel_nn<uint16_t><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_UShort2:
        transform_kernel_nn<ushort2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_UShort4:
        transform_kernel_nn<ushort4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;

    case stk::Type_Int:
        transform_kernel_nn<int><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Int2:
        transform_kernel_nn<int2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Int4:
        transform_kernel_nn<int4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;

    case stk::Type_UInt:
        transform_kernel_nn<uint32_t><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_UInt2:
        transform_kernel_nn<uint2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_UInt4:
        transform_kernel_nn<uint4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;

    case stk::Type_Float:
        transform_kernel_nn<float><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Float2:
        transform_kernel_nn<float2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Float4:
        transform_kernel_nn<float4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    default:
        FATAL() << "Unsupported format";
    };
}

static void run_linear_kernel(
    stk::Type type,
    const dim3& grid_size,
    const dim3& block_size,
    const stk::GpuVolume& src,
    const stk::GpuVolume& df,
    stk::GpuVolume& out
)
{
    float3 inv_moving_spacing = {
        1.0f / src.spacing().x,
        1.0f / src.spacing().y,
        1.0f / src.spacing().z,
    };
    
    switch (type) {
    case stk::Type_Float:
        transform_kernel_linear<float><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Float2:
        transform_kernel_linear<float2><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    case stk::Type_Float4:
        transform_kernel_linear<float4><<<grid_size, block_size>>>(
            src,
            src.size(),
            df,
            df.size(),
            df.origin(),
            df.spacing(),
            src.origin(),
            inv_moving_spacing,
            out
        );
        break;
    default:
        FATAL() << "Interpolation mode only supports float types";
    };
}


stk::GpuVolume gpu::transform_volume(
    const stk::GpuVolume& src, 
    const stk::GpuVolume& def, 
    transform::Interp i,
    const dim3& block_size
)
{
    ASSERT(def.usage() == stk::gpu::Usage_PitchedPointer);
    ASSERT(src.usage() == stk::gpu::Usage_PitchedPointer);
    FATAL_IF(def.voxel_type() != stk::Type_Float4)
        << "Invalid format for displacement";
    
    dim3 dims = def.size();

    stk::GpuVolume out(dims, src.voxel_type());
    out.copy_meta_from(def);

    dim3 grid_size {
        (dims.x + block_size.x - 1) / block_size.x,
        (dims.y + block_size.y - 1) / block_size.y,
        (dims.z + block_size.z - 1) / block_size.z
    };

    if (i == transform::Interp_NN) {
        run_nn_kernel(src.voxel_type(), grid_size, block_size, src, def, out);
    } else {
        run_linear_kernel(src.voxel_type(), grid_size, block_size, src, def, out);
    }

    CUDA_CHECK_ERRORS(cudaPeekAtLastError());
    CUDA_CHECK_ERRORS(cudaDeviceSynchronize());
    
    return out;
}