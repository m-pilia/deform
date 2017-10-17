#include "stb.h"
#include "volume.h"

#ifdef DF_BUILD_WINDOWS
    #pragma warning(push)
    #pragma warning(disable: 4456) // declaration of 'k' hides previous local declaration
    #pragma warning(disable: 4996) // 'fopen': This function or variable may be unsafe...
    #pragma warning(disable: 4244) // '=': conversion from 'int' to 'stbi__uint16', possible loss of data
#else
    // TODO: Old gcc does not support push/pop
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef DF_BUILD_WINDOWS
    #pragma warning(pop)
#endif // DF_BUILD_WINDOWS

#include <framework/debug/assert.h>
#include <framework/debug/log.h>

namespace stb
{
    Volume read_image(const char* file)
    {
        int x, y, n;
        uint8_t* data = stbi_load(file, &x, &y, &n, 0);
        if (!data)
        {
            return Volume(); // Return "invalid" volume
        }

        Dims size = { uint32_t(x), uint32_t(y), 1 };
        voxel::Type voxel_type = voxel::Type_Unknown;
        if (n == 1) voxel_type = voxel::Type_UChar;
        if (n == 2) voxel_type = voxel::Type_UChar2;
        if (n == 3) voxel_type = voxel::Type_UChar3;
        if (n == 4) voxel_type = voxel::Type_UChar4;

        Volume vol(size, voxel_type, data);

        stbi_image_free(data);
        return vol;
    }
    const char* last_read_error()
    {
        return stbi_failure_reason();
    }    
    bool write_image(const char* file, const Volume& volume)
    {
        assert(volume.voxel_type() == voxel::Type_UChar ||
               volume.voxel_type() == voxel::Type_UChar2 ||
               volume.voxel_type() == voxel::Type_UChar3 ||
               volume.voxel_type() == voxel::Type_UChar4);
        assert(volume.size().depth == 1);

        int num_comps = voxel::num_components(volume.voxel_type());
        Dims size = volume.size();

        /// Supported formats: .png, .bmp, .tga

        const char* ext = file + strlen(file) - 4;

        #ifdef DF_BUILD_WINDOWS        
            #define strcmp_ignore_case _stricmp
        #else
            #define strcmp_ignore_case strcasecmp
        #endif
        
        if (strcmp_ignore_case(ext, ".png") == 0)
        {
            int ret = stbi_write_png(file, int(size.width), int(size.height), num_comps, volume.ptr(), int(size.width * num_comps));
            assert(ret != 0);
            if(ret == 0)
            {
                LOG(Error, "Failed to write image: '%s'\n", file);
                return false;
            }
        }
        else if (strcmp_ignore_case(ext, ".bmp") == 0)
        {
            int ret = stbi_write_bmp(file, int(size.width), int(size.height), num_comps, volume.ptr());
            assert(ret != 0);
            if(ret == 0)
            {
                LOG(Error, "Failed to write image: '%s'\n", file);
                return false;
            }
        }
        else if (strcmp_ignore_case(ext, ".tga") == 0)
        {
            int ret = stbi_write_tga(file, int(size.width), int(size.height), num_comps, volume.ptr());
            assert(ret != 0);
            if(ret == 0)
            {
                LOG(Error, "Failed to write image: '%s'\n", file);
                return false;
            }
        }
        else
        {
            assert(false && "Unsupported image extension");
            return false;
        }

        #undef strcmp_ignore_case
        
        return true;
    }
}
