/********************************************************************************
*                                                                               *
*                          D D S   I n p u t / O u t p u t                      *
*                                                                               *
*********************************************************************************
* Copyright (C) 1998,2010 by Jeroen van der Zijp.   All Rights Reserved.        *
*********************************************************************************
* This library is free software; you can redistribute it and/or modify          *
* it under the terms of the GNU Lesser General Public License as published by   *
* the Free Software Foundation; either version 3 of the License, or             *
* (at your option) any later version.                                           *
*                                                                               *
* This library is distributed in the hope that it will be useful,               *
* but WITHOUT ANY WARRANTY; without even the implied warranty of                *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 *
* GNU Lesser General Public License for more details.                           *
*                                                                               *
* You should have received a copy of the GNU Lesser General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>          *
********************************************************************************/
#include "xincs.h"
#include "fxver.h"
#include "fxdefs.h"
#include "FXHash.h"
#include "FXhalf.h"
#include "FXElement.h"
#include "FXStream.h"

/*
  Notes:
  - For cubic environment maps, one or more faces of a cube are written to the file, using either uncompressed or
    compressed formats, and all faces must be the same size. Each face can have mipmaps defined, although all faces
    must have the same number of mipmap levels. If a file contains a cube map, DDSCAPS_COMPLEX, DDSCAPS2_CUBEMAP,
    and one or more of DSCAPS2_CUBEMAP_POSITIVEX/Y/Z and/or DDSCAPS2_CUBEMAP_NEGATIVEX/Y/Z should be set.
    The faces are written in the order: positive x, negative x, positive y, negative y, positive z, negative z,
    with any missing faces omitted. Each face is written with its main image, followed by any mipmap levels.
  - For a volume texture, use the DDSCAPS_COMPLEX, DDSCAPS2_VOLUME, DDSD_DEPTH, flags and set and dwDepth.
    A volume texture is an extension of a standard texture for Direct3D 9; a volume texture is can be defined with
    or without mipmaps.  For volumes without mipmaps, each depth slice is written to the file in order.
    If mipmaps are included, all depth slices for a given mipmap level are written together, with each level
    containing half as many slices as the previous level with a minimum of 1.
  - Volume textures do not support compression in Direct3D 9.
  - For an uncompressed texture, use the DDSD_PITCH and DDPF_RGB flags; for a compressed texture,
    use the DDSD_LINEARSIZE and DDPF_FOURCC flags.
  - For a mipmapped texture, use the DDSD_MIPMAPCOUNT, DDSCAPS_MIPMAP, and DDSCAPS_COMPLEX flags also as
    well as the mipmap count member. If mipmaps are generated, all levels down to 1-by-1 are usually written.
*/

// Magic file header constant
#define DDSD_MAGIC                           0x20534444

#define DDSD_CAPS                            0x00000001
#define DDSD_HEIGHT                          0x00000002
#define DDSD_WIDTH                           0x00000004
#define DDSD_PITCH                           0x00000008
#define DDSD_PIXELFORMAT	             0x00001000
#define DDSD_MIPMAPCOUNT	             0x00020000
#define DDSD_LINEARSIZE   	             0x00080000
#define DDSD_DEPTH      	             0x00800000

#define DDSF_REQUIRED                        (DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT)

// Pixel format flags
#define DDPF_ALPHAPIXELS	             0x00000001
#define DDPF_ALPHA                           0x00000002
#define DDPF_FOURCC                          0x00000004
#define DDPF_PALETTEINDEXED4                 0x00000008
#define DDPF_PALETTEINDEXED8                 0x00000020
#define DDPF_RGB                             0x00000040
#define DDPF_COMPRESSED                      0x00000080
#define DDPF_RGBTOYUV                        0x00000100
#define DDPF_YUV                             0x00000200
#define DDPF_ZBUFFER                         0x00000400
#define DDPF_PALETTEINDEXED1                 0x00000800
#define DDPF_PALETTEINDEXED2                 0x00001000
#define DDPF_ZPIXELS                         0x00002000
#define DDPF_STENCILBUFFER                   0x00004000
#define DDPF_ALPHAPREMULT                    0x00008000
#define DDPF_LUMINANCE                       0x00020000
#define DDPF_BUMPLUMINANCE                   0x00040000
#define DDPF_NORMAL                          0x80000000

// CAPS field
#define DDSCAPS_COMPLEX                      0x00000008
#define DDSCAPS_TEXTURE                      0x00001000
#define DDSCAPS_MIPMAP                       0x00400000

// CAPS2 field
#define DDSCAPS2_CUBEMAP                     0x00000200
#define DDSCAPS2_VOLUME                      0x00200000

// Cube maps
#define DDSCAPS2_CUBEMAP_POSITIVEX	     0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX	     0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY	     0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY	     0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ	     0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ	     0x00008000

// FOURCC constants
#define FOURCC_DXT1                          0x31545844         // DXT1
#define FOURCC_DXT2                          0x32545844         // DXT2
#define FOURCC_DXT3                          0x33545844         // DXT3
#define FOURCC_DXT4                          0x34545844         // DXT4
#define FOURCC_DXT5                          0x35545844         // DXT5
#define FOURCC_RXGB                          0x42475852         // RXGB (AKA DOOM III)
#define FOURCC_ATI1                          0x31495441         // ATI1
#define FOURCC_ATI2                          0x32495441         // ATI2 (AKA 3Dc)
#define FOURCC_A2XY                          0x59583241         // A2XY
#define FOURCC_DX10                          0x30315844         // DX10
#define FOURCC_R16F                          0x0000006f         // 16-bit float Red
#define FOURCC_G16R16F                       0x00000070         // 16-bit float Red/Green
#define FOURCC_A16B16G16R16F                 0x00000071         // 16-bit float RGBA
#define FOURCC_R32F                          0x00000072         // 32-bit float Red
#define FOURCC_G32R32F                       0x00000073         // 32-bit float Red/Green
#define FOURCC_A32B32G32R32F                 0x00000074         // 32-bit float RGBA

// DX10 Resource dimension
#define D3D10_RESOURCE_DIMENSION_UNKNOWN     0                  // Resource is of unknown type.
#define D3D10_RESOURCE_DIMENSION_BUFFER      1                  // Resource is a buffer.
#define D3D10_RESOURCE_DIMENSION_TEXTURE1D   2                  // Resource is a 1D texture.
#define D3D10_RESOURCE_DIMENSION_TEXTURE2D   3                  // Resource is a 2D texture.
#define D3D10_RESOURCE_DIMENSION_TEXTURE3D   4                  // Resource is a 3D texture.

// DX10 Miscellaneous flag
#define D3D10_RESOURCE_MISC_GENERATE_MIPS     0x1               // Enable mipmap generation
#define D3D10_RESOURCE_MISC_SHARED            0x2               // Enable share
#define D3D10_RESOURCE_MISC_TEXTURECUBE       0x4               // Enable cube map
#define D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX 0x10
#define D3D10_RESOURCE_MISC_GDI_COMPATIBLE    0x20




// DX10 Formats
#define DXGI_FORMAT_UNKNOWN                  0                  // The format is not known.

#define DXGI_FORMAT_R32G32B32A32_TYPELESS    1                  // A four-component, 128-bit typeless format.
#define DXGI_FORMAT_R32G32B32A32_FLOAT       2                  // A four-component, 128-bit floating-point format
#define DXGI_FORMAT_R32G32B32A32_UINT        3                  // A four-component, 128-bit unsigned-integer format
#define DXGI_FORMAT_R32G32B32A32_SINT        4                  // A four-component, 128-bit signed-integer format.

#define DXGI_FORMAT_R32G32B32_TYPELESS       5                  // A three-component, 96-bit typeless format.
#define DXGI_FORMAT_R32G32B32_FLOAT          6                  // A three-component, 96-bit floating-point format.
#define DXGI_FORMAT_R32G32B32_UINT           7                  // A three-component, 96-bit unsigned-integer format.
#define DXGI_FORMAT_R32G32B32_SINT           8                  // A three-component, 96-bit signed-integer format.

#define DXGI_FORMAT_R16G16B16A16_TYPELESS    9                  // A four-component, 64-bit typeless format.
#define DXGI_FORMAT_R16G16B16A16_FLOAT       10                 // A four-component, 64-bit floating-point format.
#define DXGI_FORMAT_R16G16B16A16_UNORM       11                 // A four-component, 64-bit unsigned-integer format.
#define DXGI_FORMAT_R16G16B16A16_UINT        12                 // A four-component, 64-bit unsigned-integer format.
#define DXGI_FORMAT_R16G16B16A16_SNORM       13                 // A four-component, 64-bit signed-integer format.
#define DXGI_FORMAT_R16G16B16A16_SINT        14                 // A four-component, 64-bit signed-integer format.

#define DXGI_FORMAT_R32G32_TYPELESS          15                 // A two-component, 64-bit typeless format.
#define DXGI_FORMAT_R32G32_FLOAT             16                 // A two-component, 64-bit floating-point format.
#define DXGI_FORMAT_R32G32_UINT              17                 // A two-component, 64-bit unsigned-integer format.
#define DXGI_FORMAT_R32G32_SINT              18                 // A two-component, 64-bit signed-integer format.

#define DXGI_FORMAT_R32G8X24_TYPELESS        19                 // A two-component, 64-bit typeless format.
#define DXGI_FORMAT_D32_FLOAT_S8X24_UINT     20                 // A 32-bit floating-point component, and two unsigned-integer components (with an additional 32 bits).
#define DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS 21                 // A 32-bit floating-point component, and two typeless components (with an additional 32 bits).
#define DXGI_FORMAT_X32_TYPELESS_G8X24_UINT  22                 // A 32-bit typeless component, and two unsigned-integer components (with an additional 32 bits).

#define DXGI_FORMAT_R10G10B10A2_TYPELESS     23                 // A four-component, 32-bit typeless format.
#define DXGI_FORMAT_R10G10B10A2_UNORM        24                 // A four-component, 32-bit unsigned-integer format.
#define DXGI_FORMAT_R10G10B10A2_UINT         25                 // A four-component, 32-bit unsigned-integer format.

#define DXGI_FORMAT_R11G11B10_FLOAT          26                 // A three-component, 32-bit floating-point format.

#define DXGI_FORMAT_R8G8B8A8_TYPELESS        27                 // A three-component, 32-bit typeless format.
#define DXGI_FORMAT_R8G8B8A8_UNORM           28                 // A four-component, 32-bit unsigned-integer format.
#define DXGI_FORMAT_R8G8B8A8_UNORM_SRGB      29                 // A four-component, 32-bit unsigned-normalized integer sRGB format.
#define DXGI_FORMAT_R8G8B8A8_UINT            30                 // A four-component, 32-bit unsigned-integer format.
#define DXGI_FORMAT_R8G8B8A8_SNORM           31                 // A three-component, 32-bit signed-integer format.
#define DXGI_FORMAT_R8G8B8A8_SINT            32                 // A three-component, 32-bit signed-integer format.

#define DXGI_FORMAT_R16G16_TYPELESS          33                 // A two-component, 32-bit typeless format.
#define DXGI_FORMAT_R16G16_FLOAT             34                 // A two-component, 32-bit floating-point format.
#define DXGI_FORMAT_R16G16_UNORM             35                 // A two-component, 32-bit unsigned-integer format.
#define DXGI_FORMAT_R16G16_UINT              36                 // A two-component, 32-bit unsigned-integer format.
#define DXGI_FORMAT_R16G16_SNORM             37                 // A two-component, 32-bit signed-integer format.
#define DXGI_FORMAT_R16G16_SINT              38                 // A two-component, 32-bit signed-integer format.

#define DXGI_FORMAT_R32_TYPELESS             39                 // A single-component, 32-bit typeless format.
#define DXGI_FORMAT_D32_FLOAT                40                 // A single-component, 32-bit floating-point format.
#define DXGI_FORMAT_R32_FLOAT                41                 // A single-component, 32-bit floating-point format.
#define DXGI_FORMAT_R32_UINT                 42                 // A single-component, 32-bit unsigned-integer format.
#define DXGI_FORMAT_R32_SINT                 43                 // A single-component, 32-bit signed-integer format.

#define DXGI_FORMAT_R24G8_TYPELESS           44                 // A two-component, 32-bit typeless format.
#define DXGI_FORMAT_D24_UNORM_S8_UINT        45                 // A 32-bit z-buffer format that uses 24 bits for the depth channel and 8 bits for the stencil channel.
#define DXGI_FORMAT_R24_UNORM_X8_TYPELESS    46                 // A 32-bit format, that contains a 24 bit, single-component, unsigned-normalized integer, with an additional typeless 8 bits.
#define DXGI_FORMAT_X24_TYPELESS_G8_UINT     47                 // A 32-bit format, that contains a 24 bit, single-component, typeless format, with an additional 8 bit unsigned integer component.

#define DXGI_FORMAT_R8G8_TYPELESS            48                 // A two-component, 16-bit typeless format.
#define DXGI_FORMAT_R8G8_UNORM               49                 // A two-component, 16-bit unsigned-integer format.
#define DXGI_FORMAT_R8G8_UINT                50                 // A two-component, 16-bit unsigned-integer format.
#define DXGI_FORMAT_R8G8_SNORM               51                 // A two-component, 16-bit signed-integer format.
#define DXGI_FORMAT_R8G8_SINT                52                 // A two-component, 16-bit signed-integer format.

#define DXGI_FORMAT_R16_TYPELESS             53                 // A single-component, 16-bit typeless format.
#define DXGI_FORMAT_R16_FLOAT                54                 // A single-component, 16-bit floating-point format.
#define DXGI_FORMAT_D16_UNORM                55                 // A single-component, 16-bit unsigned-normalized integer format.
#define DXGI_FORMAT_R16_UNORM                56                 // A single-component, 16-bit unsigned-integer format.
#define DXGI_FORMAT_R16_UINT                 57                 // A single-component, 16-bit unsigned-integer format.
#define DXGI_FORMAT_R16_SNORM                58                 // A single-component, 16-bit signed-integer format.
#define DXGI_FORMAT_R16_SINT                 59                 // A single-component, 16-bit signed-integer format.

#define DXGI_FORMAT_R8_TYPELESS              60                 // A single-component, 8-bit typeless format.
#define DXGI_FORMAT_R8_UNORM                 61                 // A single-component, 8-bit unsigned-integer format.
#define DXGI_FORMAT_R8_UINT                  62                 // A single-component, 8-bit unsigned-integer format.
#define DXGI_FORMAT_R8_SNORM                 63                 // A single-component, 8-bit signed-integer format.
#define DXGI_FORMAT_R8_SINT                  64                 // A single-component, 8-bit signed-integer format.
#define DXGI_FORMAT_A8_UNORM                 65                 // A single-component, 8-bit unsigned-integer format.

#define DXGI_FORMAT_R1_UNORM                 66                 // A single-component, 1-bit unsigned-normalized integer format.

#define DXGI_FORMAT_R9G9B9E5_SHAREDEXP       67                 // A four-component, 32-bit floating-point format.

#define DXGI_FORMAT_R8G8_B8G8_UNORM          68                 // A four-component, 32-bit unsigned-normalized integer format.
#define DXGI_FORMAT_G8R8_G8B8_UNORM          69                 // A four-component, 32-bit unsigned-normalized integer format.

#define DXGI_FORMAT_BC1_TYPELESS             70                 // 4-channel typeless block-compression format.
#define DXGI_FORMAT_BC1_UNORM                71                 // 4-channel block-compression format.
#define DXGI_FORMAT_BC1_UNORM_SRGB           72                 // 4-channel block-compression format for sRGB data.

#define DXGI_FORMAT_BC2_TYPELESS             73                 // 4-channel typeless block-compression format.
#define DXGI_FORMAT_BC2_UNORM                74                 // 4-channel block-compression format.
#define DXGI_FORMAT_BC2_UNORM_SRGB           75                 // 4-channel block-compression format for sRGB data.

#define DXGI_FORMAT_BC3_TYPELESS             76                 // 4-channel typeless block-compression format.
#define DXGI_FORMAT_BC3_UNORM                77                 // 4-channel block-compression format.
#define DXGI_FORMAT_BC3_UNORM_SRGB           78                 // 4-channel block-compression format for sRGB data.

#define DXGI_FORMAT_BC4_TYPELESS             79                 // 1-channel typeless block-compression format.
#define DXGI_FORMAT_BC4_UNORM                80                 // 1-channel block-compression format.
#define DXGI_FORMAT_BC4_SNORM                81                 // 1-channel block-compression format.

#define DXGI_FORMAT_BC5_TYPELESS             82                 // 2-channel typeless block-compression format.
#define DXGI_FORMAT_BC5_UNORM                83                 // 2-channel block-compression format.
#define DXGI_FORMAT_BC5_SNORM                84                 // 2-channel block-compression format.

#define DXGI_FORMAT_B5G6R5_UNORM             85                 // A three-component, 16-bit unsigned-normalized integer format.
#define DXGI_FORMAT_B5G5R5A1_UNORM           86                 // A four-component, 16-bit unsigned-normalized integer format that supports 1-bit alpha.
#define DXGI_FORMAT_B8G8R8A8_UNORM           87                 // A four-component, 16-bit unsigned-normalized integer format that supports 8-bit alpha.
#define DXGI_FORMAT_B8G8R8X8_UNORM           88                 // A four-component, 16-bit unsigned-normalized integer format.



using namespace FX;

/*******************************************************************************/

namespace FX {

extern FXAPI FXbool fxcheckDDS(FXStream& store);
extern FXAPI FXbool fxloadDDS(FXStream& store,FXColor*& data,FXint& width,FXint& height,FXint& depth);
extern FXAPI FXbool fxsaveDDS(FXStream& store,FXColor* data,FXint width,FXint height,FXint depth);


// Pixel format
struct DDSPixelFormat {
  FXuint         dwSize;
  FXuint         dwFlags;
  FXuint         dwFourCC;
  FXuint         dwRGBBitCount;
  FXuint         dwRBitMask;
  FXuint         dwGBitMask;
  FXuint         dwBBitMask;
  FXuint         dwABitMask;
  };


// DDS Header
struct DDSHeader {
  FXuint         dwSize;
  FXuint         dwFlags;
  FXuint         dwHeight;
  FXuint         dwWidth;
  FXuint         dwLinearSize;
  FXuint         dwDepth;
  FXuint         dwMipMapCount;
  FXuint         dwReserved1[11];
  DDSPixelFormat ddpf;
  FXuint         dwCaps;
  FXuint         dwCaps2;
  FXuint         dwCaps3;
  FXuint         dwCaps4;
  FXuint         dwReserved2;
  };


// DX10 extra header structure
struct DDSXHeader {
  FXuint         dxgiFormat;
  FXuint         resourceDimension;
  FXuint         miscFlag;
  FXuint         arraySize;
  FXuint         reserved;
  };


// DDS Image
struct DDSImage {
  FXuint       magic;
  DDSHeader    header;
  DDSXHeader   xheader;
  FXuchar     *data;
  };


// Undo premultiplied alpha
// The math: 255*X = (255*R * 255*A)/255, so 255*R = 255*X*255 / 255*A
static void dds_correct_color(FXuchar *image,FXuint size){
  register FXuint i,a;
  for(i=0; i<size; i+=4){
    if((a=image[i+3])>0){
      image[i+0]=(image[i+0]*255)/a;
      image[i+1]=(image[i+1]*255)/a;
      image[i+2]=(image[i+2]*255)/a;
      }
    }
  }


// Swizzle red and alpha for RXGB
static void dds_correct_swizzle(FXuchar *image,FXuint size){
  register FXuint i;
  for(i=0; i<size; i+=4){
    image[i+0]^=image[i+3];
    image[i+3]^=image[i+0];
    image[i+0]^=image[i+3];
    }
  }


// Decompress DXT1 image
static FXbool dds_decompress_DXT1(const DDSImage& dds,FXuchar *image){
  register const FXuchar *temp=dds.data;
  register FXuint x,y,z,i,j,select,bitmask,offset;
  register FXuchar r0,g0,b0,r1,g1,b1;
  register FXushort c0,c1;
  FXuchar colors[4][4];

  // Loop over 4x4 blocks
  for(z=0; z<dds.header.dwDepth; z+=1){
    for(y=0; y<dds.header.dwHeight; y+=4){
      for(x=0; x<dds.header.dwWidth; x+=4){

        // Grab two 5,6,5 colors
        c0=(((FXushort)temp[1])<<8) | (FXushort)temp[0];
        c1=(((FXushort)temp[3])<<8) | (FXushort)temp[2];

        r0=(c0>>11)&0x1f;
        g0=(c0>>5)&0x3f;
        b0=c0&0x1f;

        r1=(c1>>11)&0x1f;
        g1=(c1>>5)&0x3f;
        b1=c1&0x1f;

        colors[0][0]=(r0<<3)|(r0>>2);     // Convert from 5,6,5 to 8,8,8 color #0
        colors[0][1]=(g0<<2)|(g0>>4);
        colors[0][2]=(b0<<3)|(b0>>2);
        colors[0][3]=0xFF;

        colors[1][0]=(r1<<3)|(r1>>2);     // Convert from 5,6,5 to 8,8,8 color #1
        colors[1][1]=(g1<<2)|(g1>>4);
        colors[1][2]=(b1<<3)|(b1>>2);
        colors[1][3]=0xFF;

        // Four color block: 00=color #0, 01=color #1, 10=color #2, 11=color #3
        if(c0>c1){
          colors[2][0]=(2*colors[0][0]+colors[1][0]+1)/3;
          colors[2][1]=(2*colors[0][1]+colors[1][1]+1)/3;
          colors[2][2]=(2*colors[0][2]+colors[1][2]+1)/3;
          colors[2][3]=255;
          colors[3][0]=(colors[0][0]+2*colors[1][0]+1)/3;
          colors[3][1]=(colors[0][1]+2*colors[1][1]+1)/3;
          colors[3][2]=(colors[0][2]+2*colors[1][2]+1)/3;
          colors[3][3]=255;
          }

        // Three color block: 00=color #0, 01=color #1, 10=color #2, 11=transparent
        else{
          colors[2][0]=(colors[0][0]+colors[1][0])/2;
          colors[2][1]=(colors[0][1]+colors[1][1])/2;
          colors[2][2]=(colors[0][2]+colors[1][2])/2;
          colors[2][3]=255;
          colors[3][0]=0;
          colors[3][1]=0;
          colors[3][2]=0;
          colors[3][3]=0;
          }

        // Get index bits all at once
        bitmask=(((FXuint)temp[7])<<24)|(((FXuint)temp[6])<<16)|(((FXuint)temp[5])<<8)|((FXuint)temp[4]);

        // Decode the bits
        for(j=0; j<4; ++j){
          for(i=0; i<4; ++i){
            if(((x+i)<dds.header.dwWidth) && ((y+j)<dds.header.dwHeight)){
              select=bitmask&3;
              offset=((z*dds.header.dwHeight+y+j)*dds.header.dwWidth+x+i)<<2;
              image[offset+0]=colors[select][0];
              image[offset+1]=colors[select][1];
              image[offset+2]=colors[select][2];
              image[offset+3]=colors[select][3];
              }
            bitmask>>=2;
            }
          }
        temp+=8;
        }
      }
    }
  return true;
  }


// Decompress DXT3 image
static FXbool dds_decompress_DXT3(const DDSImage& dds,FXuchar *image){
  register FXuchar *temp=dds.data;
  register FXuint x,y,z,i,j,select,bitmask,offset;
  register FXuchar r0,g0,b0,r1,g1,b1;
  register FXushort c0,c1;
  FXuchar colors[4][4];
  FXuchar alpha[4][4];

  // Loop over 4x4 blocks
  for(z=0; z<dds.header.dwDepth; z+=1){
    for(y=0; y<dds.header.dwHeight; y+=4){
      for(x=0; x<dds.header.dwWidth; x+=4){

        // Grab 16 4-bit alpha values and convert them to 8-bit ones
        alpha[0][0]=(temp[0]&15)*17;
        alpha[0][1]=(temp[0]>>4)*17;
        alpha[0][2]=(temp[1]&15)*17;
        alpha[0][3]=(temp[1]>>4)*17;

        alpha[1][0]=(temp[2]&15)*17;
        alpha[1][1]=(temp[2]>>4)*17;
        alpha[1][2]=(temp[3]&15)*17;
        alpha[1][3]=(temp[3]>>4)*17;

        alpha[2][0]=(temp[4]&15)*17;
        alpha[2][1]=(temp[4]>>4)*17;
        alpha[2][2]=(temp[5]&15)*17;
        alpha[2][3]=(temp[5]>>4)*17;

        alpha[3][0]=(temp[6]&15)*17;
        alpha[3][1]=(temp[6]>>4)*17;
        alpha[3][2]=(temp[7]&15)*17;
        alpha[3][3]=(temp[7]>>4)*17;

        // Grab two 5,6,5 colors
        c0=(((FXushort)temp[9])<<8) | (FXushort)temp[8];
        c1=(((FXushort)temp[11])<<8) | (FXushort)temp[10];

        r0=(c0>>11)&0x1f;
        g0=(c0>>5)&0x3f;
        b0=c0&0x1f;

        r1=(c1>>11)&0x1f;
        g1=(c1>>5)&0x3f;
        b1=c1&0x1f;

        colors[0][0]=(r0<<3)|(r0>>2);     // Convert from 5,6,5 to 8,8,8 color #0
        colors[0][1]=(g0<<2)|(g0>>4);
        colors[0][2]=(b0<<3)|(b0>>2);
        colors[0][3]=0xFF;

        colors[1][0]=(r1<<3)|(r1>>2);     // Convert from 5,6,5 to 8,8,8 color #1
        colors[1][1]=(g1<<2)|(g1>>4);
        colors[1][2]=(b1<<3)|(b1>>2);
        colors[1][3]=0xFF;

        // Four color block: 00=color #0, 01=color #1, 10=color #2, 11=color #3
        colors[2][0]=(2*colors[0][0]+colors[1][0]+1)/3;
        colors[2][1]=(2*colors[0][1]+colors[1][1]+1)/3;
        colors[2][2]=(2*colors[0][2]+colors[1][2]+1)/3;
        colors[2][3]=0xFF;

        colors[3][0]=(colors[0][0]+2*colors[1][0]+1)/3;
        colors[3][1]=(colors[0][1]+2*colors[1][1]+1)/3;
        colors[3][2]=(colors[0][2]+2*colors[1][2]+1)/3;
        colors[3][3]=0xFF;

        // Get index bits all at once
        bitmask=(((FXuint)temp[15])<<24)|(((FXuint)temp[14])<<16)|(((FXuint)temp[13])<<8)|((FXuint)temp[12]);

        // Decode the bits
        for(j=0; j<4; ++j){
          for(i=0; i<4; ++i){
            if(((x+i)<dds.header.dwWidth) && ((y+j)<dds.header.dwHeight)){
              select=bitmask&3;
              offset=((z*dds.header.dwHeight+y+j)*dds.header.dwWidth+x+i)<<2;
              image[offset+0]=colors[select][0];
              image[offset+1]=colors[select][1];
              image[offset+2]=colors[select][2];
              image[offset+3]=alpha[j][i];
              }
            bitmask>>=2;
            }
          }
        temp+=16;
        }
      }
    }
  return true;
  }


// Decompress DXT2 image; has premultiplied alpha
static FXbool dds_decompress_DXT2(const DDSImage& dds,FXuchar *image){
  if(dds_decompress_DXT3(dds,image)){
    dds_correct_color(image,dds.header.dwWidth*dds.header.dwHeight*dds.header.dwDepth*4);
    return true;
    }
  return false;
  }


// Decompress DXT5 image
static FXbool dds_decompress_DXT5(const DDSImage& dds,FXuchar *image){
  register FXuchar *temp=dds.data;
  register FXuint x,y,z,i,j,select,bitmask,bits,offset;
  register FXuchar r0,g0,b0,r1,g1,b1;
  register FXushort c0,c1;
  FXuchar colors[4][4];
  FXuchar levels[8];
  FXuchar alpha[4][4];

  // Loop over 4x4 blocks
  for(z=0; z<dds.header.dwDepth; z+=1){
    for(y=0; y<dds.header.dwHeight; y+=4){
      for(x=0; x<dds.header.dwWidth; x+=4){

        // Grab two 8-bit alphas
        levels[0]=temp[0];
        levels[1]=temp[1];

        // Six interpolated alpha levels
        if(levels[0]>levels[1]){
          levels[2]=(6*levels[0]+1*levels[1]+3)/7;                // bit code 010
          levels[3]=(5*levels[0]+2*levels[1]+3)/7;                // bit code 011
          levels[4]=(4*levels[0]+3*levels[1]+3)/7;                // bit code 100
          levels[5]=(3*levels[0]+4*levels[1]+3)/7;                // bit code 101
          levels[6]=(2*levels[0]+5*levels[1]+3)/7;                // bit code 110
          levels[7]=(1*levels[0]+6*levels[1]+3)/7;                // bit code 111
          }

        // 4 interpolated alpha levels
        else{
          levels[2]=(4*levels[0]+1*levels[1]+2)/5;                // Bit code 010
          levels[3]=(3*levels[0]+2*levels[1]+2)/5;                // Bit code 011
          levels[4]=(2*levels[0]+3*levels[1]+2)/5;                // Bit code 100
          levels[5]=(1*levels[0]+4*levels[1]+2)/5;                // Bit code 101
          levels[6]=0x00;                                         // Bit code 110
          levels[7]=0xFF;                                         // Bit code 111
          }

        // First three bytes
        bits=(((FXuint)temp[4])<<16)|(((FXuint)temp[3])<<8)|((FXuint)temp[2]);
        for(j=0; j<2; ++j){
          for(i=0; i<4; ++i){
            alpha[j][i]=levels[bits&7];
            bits>>=3;
            }
          }

        // Last three bytes
        bits=(((FXuint)temp[7])<<16)|(((FXuint)temp[6])<<8)|((FXuint)temp[5]);
        for(j=2; j<4; ++j){
          for(i=0; i<4; ++i){
            alpha[j][i]=levels[bits&7];
            bits>>=3;
            }
          }

        // Grab two 5,6,5 colors
        c0=(((FXushort)temp[9])<<8) | (FXushort)temp[8];
        c1=(((FXushort)temp[11])<<8) | (FXushort)temp[10];

        r0=(c0>>11)&0x1f;
        g0=(c0>>5)&0x3f;
        b0=c0&0x1f;

        r1=(c1>>11)&0x1f;
        g1=(c1>>5)&0x3f;
        b1=c1&0x1f;

        colors[0][0]=(r0<<3)|(r0>>2);     // Convert from 5,6,5 to 8,8,8 color #0
        colors[0][1]=(g0<<2)|(g0>>4);
        colors[0][2]=(b0<<3)|(b0>>2);
        colors[0][3]=0xFF;

        colors[1][0]=(r1<<3)|(r1>>2);     // Convert from 5,6,5 to 8,8,8 color #1
        colors[1][1]=(g1<<2)|(g1>>4);
        colors[1][2]=(b1<<3)|(b1>>2);
        colors[1][3]=0xFF;

        // Four color block: 00=color #0, 01=color #1, 10=color #2, 11=color #3
        colors[2][0]=(2*colors[0][0]+colors[1][0]+1)/3;
        colors[2][1]=(2*colors[0][1]+colors[1][1]+1)/3;
        colors[2][2]=(2*colors[0][2]+colors[1][2]+1)/3;
        colors[2][3]=0xFF;

        colors[3][0]=(colors[0][0]+2*colors[1][0]+1)/3;
        colors[3][1]=(colors[0][1]+2*colors[1][1]+1)/3;
        colors[3][2]=(colors[0][2]+2*colors[1][2]+1)/3;
        colors[3][3]=0xFF;

        // Get index bits all at once
        bitmask=(((FXuint)temp[15])<<24)|(((FXuint)temp[14])<<16)|(((FXuint)temp[13])<<8)|((FXuint)temp[12]);

        // Decode the bits
        for(j=0; j<4; ++j){
          for(i=0; i<4; ++i){
            if(((x+i)<dds.header.dwWidth) && ((y+j)<dds.header.dwHeight)){
              select=bitmask&3;
              offset=((z*dds.header.dwHeight+y+j)*dds.header.dwWidth+x+i)<<2;
              image[offset+0]=colors[select][0];
              image[offset+1]=colors[select][1];
              image[offset+2]=colors[select][2];
              image[offset+3]=alpha[j][i];
              }
            bitmask>>=2;
            }
          }
        temp+=16;
        }
      }
    }
  return true;
  }


// Decompress DXT4 image; has premultiplied alpha
static FXbool dds_decompress_DXT4(const DDSImage& dds,FXuchar *image){
  if(dds_decompress_DXT5(dds,image)){
    dds_correct_color(image,dds.header.dwWidth*dds.header.dwHeight*dds.header.dwDepth*4);
    return true;
    }
  return false;
  }


// Decompress RXGB image
static FXbool dds_decompress_RXGB(const DDSImage& dds,FXuchar *image){
  if(dds_decompress_DXT5(dds,image)){
    dds_correct_swizzle(image,dds.header.dwWidth*dds.header.dwHeight*dds.header.dwDepth*4);
    return true;
    }
  return false;
  }


// Decompress BC4 (ATI1) image
static FXbool dds_decompress_BC4(const DDSImage& dds,FXuchar *image){
  register FXuchar *temp=dds.data;
  register FXuint x,y,z,i,j,bits,offset;
  FXuchar levels[8];

  // Loop over 4x4 blocks
  for(z=0; z<dds.header.dwDepth; z+=1){
    for(y=0; y<dds.header.dwHeight; y+=4){
      for(x=0; x<dds.header.dwWidth; x+=4){

        // Grab two 8-bit grey levels
        levels[0]=temp[0];
        levels[1]=temp[1];

        // Six interpolated grey levels
        if(levels[0]>levels[1]){
          levels[2]=(6*levels[0]+1*levels[1]+3)/7;
          levels[3]=(5*levels[0]+2*levels[1]+3)/7;
          levels[4]=(4*levels[0]+3*levels[1]+3)/7;
          levels[5]=(3*levels[0]+4*levels[1]+3)/7;
          levels[6]=(2*levels[0]+5*levels[1]+3)/7;
          levels[7]=(1*levels[0]+6*levels[1]+3)/7;
          }

        // 4 interpolated grey levels
        else{
          levels[2]=(4*levels[0]+1*levels[1]+2)/5;
          levels[3]=(3*levels[0]+2*levels[1]+2)/5;
          levels[4]=(2*levels[0]+3*levels[1]+2)/5;
          levels[5]=(1*levels[0]+4*levels[1]+2)/5;
          levels[6]=0;
          levels[7]=255;
          }

        // First three bytes
        bits=(((FXuint)temp[4])<<16)|(((FXuint)temp[3])<<8)|((FXuint)temp[2]);
        for(j=0; j<2; ++j){
          for(i=0; i<4; ++i){
            if(((x+i)<dds.header.dwWidth) && ((y+j)<dds.header.dwHeight)){
              offset=((z*dds.header.dwHeight+y+j)*dds.header.dwWidth+x+i)<<2;
              image[offset+0]=image[offset+1]=image[offset+2]=image[offset+3]=levels[bits&7];
              }
            bits>>=3;
            }
          }

        // Last three bytes
        bits=(((FXuint)temp[7])<<16)|(((FXuint)temp[6])<<8)|((FXuint)temp[5]);
        for(j=2; j<4; ++j){
          for(i=0; i<4; ++i){
            if(((x+i)<dds.header.dwWidth) && ((y+j)<dds.header.dwHeight)){
              offset=((z*dds.header.dwHeight+y+j)*dds.header.dwWidth+x+i)<<2;
              image[offset+0]=image[offset+1]=image[offset+2]=image[offset+3]=levels[bits&7];
              }
            bits>>=3;
            }
          }
        temp+=8;
        }
      }
    }
  return true;
  }


// Fast integer square root
static FXuint isqrt(FXuint val){
  FXuint temp,g=0,b=0x8000,bshft=15;
  do{if(val>=(temp=(((g<<1)+b)<<bshft--))){g+=b;val-=temp;}}while(b>>=1);
  return g;
  }


// Decompress 3DC (ATI2) image
static FXbool dds_decompress_3DC(const DDSImage& dds,FXuchar *image){
  register FXuchar *temp=dds.data;
  register FXuint x,y,z,i,j,redbits,grnbits,offset;
  register FXint tx,ty,t;
  FXuchar red[8];
  FXuchar grn[8];

  // Loop over 4x4 blocks
  for(z=0; z<dds.header.dwDepth; z+=1){
    for(y=0; y<dds.header.dwHeight; y+=4){
      for(x=0; x<dds.header.dwWidth; x+=4){

        // Grab two reds
        red[0]=temp[0];
        red[1]=temp[1];

        // Six interpolated values
        if(red[0]>red[1]){
          red[2]=(6*red[0]+1*red[1]+3)/7;
          red[3]=(5*red[0]+2*red[1]+3)/7;
          red[4]=(4*red[0]+3*red[1]+3)/7;
          red[5]=(3*red[0]+4*red[1]+3)/7;
          red[6]=(2*red[0]+5*red[1]+3)/7;
          red[7]=(1*red[0]+6*red[1]+3)/7;
          }

        // Four interpolated values
        else{
          red[2]=(4*red[0]+1*red[1]+2)/5;
          red[3]=(3*red[0]+2*red[1]+2)/5;
          red[4]=(2*red[0]+3*red[1]+2)/5;
          red[5]=(1*red[0]+4*red[1]+2)/5;
          red[6]=0;
          red[7]=255;
          }

        // Grab two greens
        grn[0]=temp[8];
        grn[1]=temp[9];

        // Six interpolated values
        if(grn[0]>grn[1]){
          grn[2]=(6*grn[0]+1*grn[1]+3)/7;
          grn[3]=(5*grn[0]+2*grn[1]+3)/7;
          grn[4]=(4*grn[0]+3*grn[1]+3)/7;
          grn[5]=(3*grn[0]+4*grn[1]+3)/7;
          grn[6]=(2*grn[0]+5*grn[1]+3)/7;
          grn[7]=(1*grn[0]+6*grn[1]+3)/7;
          }

        // Four interpolated values
        else{
          grn[2]=(4*grn[0]+1*grn[1]+2)/5;
          grn[3]=(3*grn[0]+2*grn[1]+2)/5;
          grn[4]=(2*grn[0]+3*grn[1]+2)/5;
          grn[5]=(1*grn[0]+4*grn[1]+2)/5;
          grn[6]=0;
          grn[7]=255;
          }

        // Decode the first 3 bytes
        redbits=(((FXuint)temp[4])<<16)|(((FXuint)temp[3])<<8)|((FXuint)temp[2]);
        grnbits=(((FXuint)temp[12])<<16)|(((FXuint)temp[11])<<8)|((FXuint)temp[10]);
        for(j=0; j<2; ++j){
          for(i=0; i<4; ++i){
            if(((x+i)<dds.header.dwWidth) && ((y+j)<dds.header.dwHeight)){
              offset=((z*dds.header.dwHeight+y+j)*dds.header.dwWidth+x+i)<<2;
              image[offset+0]=tx=red[redbits&7];
              image[offset+1]=ty=grn[grnbits&7];
              t=127*128-(tx-127)*(tx-128)-(ty-127)*(ty-128);
              if(t>0){
                image[offset+2]=(FXuchar)(isqrt(t)+128);
                }
              else{
                image[offset+2]=127;
                }
              image[offset+3]=255;
              }
            redbits>>=3;
            grnbits>>=3;
            }
          }

        // Decode the last 3 bytes
        redbits=(((FXuint)temp[7])<<16)|(((FXuint)temp[6])<<8)|((FXuint)temp[5]);
        grnbits=(((FXuint)temp[15])<<16)|(((FXuint)temp[14])<<8)|((FXuint)temp[13]);
        for(j=2; j<4; ++j){
          for(i=0; i<4; ++i){
            if(((x+i)<dds.header.dwWidth) && ((y+j)<dds.header.dwHeight)){
              offset=((z*dds.header.dwHeight+y+j)*dds.header.dwWidth+x+i)<<2;
              image[offset+0]=tx=red[redbits&7];
              image[offset+1]=ty=grn[grnbits&7];
              t=127*128-(tx-127)*(tx-128)-(ty-127)*(ty-128);
              if(t>0){
                image[offset+2]=(FXuchar)(isqrt(t)+128);
                }
              else{
                image[offset+2]=127;
                }
              image[offset+3]=255;
              }
            redbits>>=3;
            grnbits>>=3;
            }
          }
        temp+=16;
        }
      }
    }
  return true;
  }


// Compute shifts
static void getShifts(FXuint mask,FXuint& shift,FXuint& mul,FXuint& sc){
  register FXuint bits=0;
  shift=0;
  mul=1;
  sc=0;
  while(!(mask&1)){
    mask>>=1;
    shift++;
    }
  while(mask&(1<<bits)) bits++;
  while((mask*mul)<255){
    mul=(mul<<bits)+1;
    }
  mask*=mul;
  while((mask&~0xff)!=0){
    mask>>=1;
    sc++;
    }
  }


// Decompress RGB
static FXbool dds_decompress_RGB(const DDSImage& dds,FXuchar *image){
  FXuint rshift=0,gshift=0,bshift=0,ashift=0,rmul=0,gmul=0,bmul=0,amul=0,rs=0,gs=0,bs=0,as=0;
  FXuint rmask,gmask,bmask,amask,x,y,z,offset,s,pix,t;
  FXuchar *temp=dds.data;

  // Number of bytes per pixel
  s=(dds.header.ddpf.dwRGBBitCount+7)/8;

  // Grab mask values
  rmask=dds.header.ddpf.dwRBitMask;
  gmask=dds.header.ddpf.dwGBitMask;
  bmask=dds.header.ddpf.dwBBitMask;
  amask=dds.header.ddpf.dwABitMask;

  // Get shifts
  if(rmask){ getShifts(rmask,rshift,rmul,rs); }
  if(gmask){ getShifts(gmask,gshift,gmul,gs); }
  if(bmask){ getShifts(bmask,bshift,bmul,bs); }
  if(amask){ getShifts(amask,ashift,amul,as); }

  FXTRACE((200,"rmask=0x%08x rshift=%2d rmul=%3d rs=%3d\n",rmask,rshift,rmul,rs));
  FXTRACE((200,"gmask=0x%08x gshift=%2d gmul=%3d gs=%3d\n",gmask,gshift,gmul,gs));
  FXTRACE((200,"bmask=0x%08x bshift=%2d bmul=%3d bs=%3d\n",bmask,bshift,bmul,bs));
  FXTRACE((200,"amask=0x%08x ashift=%2d amul=%3d as=%3d\n",amask,ashift,amul,as));

  // Loop over pixels
  for(z=offset=0; z<dds.header.dwDepth; ++z){
    for(y=0; y<dds.header.dwHeight; ++y){
      for(x=0; x<dds.header.dwWidth; ++x){
        pix=(((FXuint)temp[3])<<24)|(((FXuint)temp[2])<<16)|(((FXuint)temp[1])<<8)|((FXuint)temp[0]);
        t=(pix&rmask)>>rshift; image[offset+0]=(t*rmul)>>rs;
        t=(pix&gmask)>>gshift; image[offset+1]=(t*gmul)>>gs;
        t=(pix&bmask)>>bshift; image[offset+2]=(t*bmul)>>bs;
        t=(pix&amask)>>ashift; image[offset+3]=(t*amul)>>as;
        offset+=4;
        temp+=s;
        }
      }
    }
  return true;
  }


// Decompress Luminance
static FXbool dds_decompress_LUM(const DDSImage& dds,FXuchar *image){
  FXuint shift=0,mul=0,gs=0;
  FXuint mask,x,y,z,offset,s,pix,t;
  FXuchar *temp=dds.data;

  // Number of bytes per pixel
  s=(dds.header.ddpf.dwRGBBitCount+7)/8;

  // Grab mask value
  mask=dds.header.ddpf.dwRBitMask;

  // Get shifts
  if(mask){ getShifts(mask,shift,mul,gs); }

  FXTRACE((1,"mask=0x%08x shift=%2d mul=%3d s=%3d\n",mask,shift,mul,gs));

  // Loop over pixels
  for(z=offset=0; z<dds.header.dwDepth; ++z){
    for(y=0; y<dds.header.dwHeight; ++y){
      for(x=0; x<dds.header.dwWidth; ++x){
        pix=(((FXuint)temp[3])<<24)|(((FXuint)temp[2])<<16)|(((FXuint)temp[1])<<8)|((FXuint)temp[0]);
        t=(pix&mask)>>shift;
        image[offset+0]=image[offset+1]=image[offset+2]=image[offset+3]=(t*mul)>>gs;
        offset+=4;
        temp+=s;
        }
      }
    }
  return true;
  }


// Decompress R16F
static FXbool dds_decompress_R16F(const DDSImage& dds,FXuchar *image){
  register FXuint count=dds.header.dwDepth*dds.header.dwHeight*dds.header.dwWidth*4;
  register FXhalf *temp=(FXhalf*)dds.data;
  register FXuint p=0;
  while(p<count){
    image[p+0]=image[p+1]=image[p+2]=image[p+3]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    p+=4;
    }
  return true;
  }


// Decompress G16R16F
static FXbool dds_decompress_G16R16F(const DDSImage& dds,FXuchar *image){
  register FXuint count=dds.header.dwDepth*dds.header.dwHeight*dds.header.dwWidth*4;
  register FXhalf *temp=(FXhalf*)dds.data;
  register FXuint p=0;
  while(p<count){
    image[p+0]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+1]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+2]=0;
    image[p+3]=255;
    p+=4;
    }
  return true;
  }


// Decompress A16B16G16R16F
static FXbool dds_decompress_A16B16G16R16F(const DDSImage& dds,FXuchar *image){
  register FXuint count=dds.header.dwDepth*dds.header.dwHeight*dds.header.dwWidth*4;
  register FXhalf *temp=(FXhalf*)dds.data;
  register FXuint p=0;
  while(p<count){
    image[p+0]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+1]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+2]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+3]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    p+=4;
    }
  return true;
  }


// Decompress R32F
static FXbool dds_decompress_R32F(const DDSImage& dds,FXuchar *image){
  register FXuint count=dds.header.dwDepth*dds.header.dwHeight*dds.header.dwWidth*4;
  register FXfloat *temp=(FXfloat*)dds.data;
  register FXuint p=0;
  while(p<count){
    image[p+0]=image[p+1]=image[p+2]=image[p+3]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    p+=4;
    }
  return true;
  }


// Decompress G32R32F
static FXbool dds_decompress_G32R32F(const DDSImage& dds,FXuchar *image){
  register FXuint count=dds.header.dwDepth*dds.header.dwHeight*dds.header.dwWidth*4;
  register FXfloat *temp=(FXfloat*)dds.data;
  register FXuint p=0;
  while(p<count){
    image[p+0]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+1]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+2]=0;
    image[p+3]=255;
    p+=4;
    }
  return true;
  }


// Decompress A32B32G32R32F
static FXbool dds_decompress_A32B32G32R32F(const DDSImage& dds,FXuchar *image){
  register FXuint count=dds.header.dwDepth*dds.header.dwHeight*dds.header.dwWidth*4;
  register FXfloat *temp=(FXfloat*)dds.data;
  register FXuint p=0;
  while(p<count){
    image[p+0]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+1]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+2]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    image[p+3]=(FXuchar)(*temp++ * 255.0f + 0.5f);
    p+=4;
    }
  return true;
  }


// Check if stream contains a BMP
FXbool fxcheckDDS(FXStream& store){
  FXuchar signature[4];
  store.load(signature,4);
  store.position(-4,FXFromCurrent);
  return signature[0]=='D' && signature[1]=='D' && signature[2]=='S' && signature[3]==' ';
  }


// Load image from stream
FXbool fxloadDDS(FXStream& store,FXColor*& data,FXint& width,FXint& height,FXint& depth){
  FXbool swap=store.swapBytes();
  FXuint blocksize;
  FXbool ok=false;
  DDSImage dds;

  // Null out
  data=NULL;
  width=0;
  height=0;
  depth=0;

  // Bitmaps are little-endian
  store.setBigEndian(false);

  // Check header
  store >> dds.magic;
  if(dds.magic==DDSD_MAGIC){
    store >> dds.header.dwSize;
    if(dds.header.dwSize!=124) goto x;                  // Unexpected size; bail!
    store >> dds.header.dwFlags;
    if((dds.header.dwFlags&DDSF_REQUIRED)!=DDSF_REQUIRED) goto x;       // Minimum set of flags required
    store >> dds.header.dwHeight;
    store >> dds.header.dwWidth;
    store >> dds.header.dwLinearSize;
    store >> dds.header.dwDepth;
    store >> dds.header.dwMipMapCount;
    store.load(dds.header.dwReserved1,11);
    store >> dds.header.ddpf.dwSize;
  //if(dds.header.ddpf.dwSize!=32) goto x;              // Unexpected size; bail!
    store >> dds.header.ddpf.dwFlags;
    store >> dds.header.ddpf.dwFourCC;
    store >> dds.header.ddpf.dwRGBBitCount;
    store >> dds.header.ddpf.dwRBitMask;
    store >> dds.header.ddpf.dwGBitMask;
    store >> dds.header.ddpf.dwBBitMask;
    store >> dds.header.ddpf.dwABitMask;
    store >> dds.header.dwCaps;
    store >> dds.header.dwCaps2;
    store >> dds.header.dwCaps3;
    store >> dds.header.dwCaps4;
    store >> dds.header.dwReserved2;

    // Load DX10 Header if present
    if(dds.header.ddpf.dwFourCC==FOURCC_DX10){          // Parse over DX10 header
      store >> dds.xheader.dxgiFormat;
      store >> dds.xheader.resourceDimension;
      store >> dds.xheader.miscFlag;
      store >> dds.xheader.arraySize;
      store >> dds.xheader.reserved;
      }
    else{
      dds.xheader.dxgiFormat=DXGI_FORMAT_UNKNOWN;
      dds.xheader.resourceDimension=D3D10_RESOURCE_DIMENSION_UNKNOWN;
      dds.xheader.miscFlag=0;
      dds.xheader.arraySize=1;
      dds.xheader.reserved=0;
      }

    FXTRACE((100,"dds.magic=0x%08x\n",dds.magic));
    FXTRACE((100,"dds.header.dwSize=%d\n",dds.header.dwSize));
    FXTRACE((100,"dds.header.dwFlags=0x%08x: %s%s%s%s%s%s%s%s\n",dds.header.dwFlags,(dds.header.dwFlags&DDSD_CAPS)?"DDSD_CAPS ":"",(dds.header.dwFlags&DDSD_HEIGHT)?"DDSD_HEIGHT ":"",(dds.header.dwFlags&DDSD_WIDTH)?"DDSD_WIDTH ":"",(dds.header.dwFlags&DDSD_PITCH)?"DDSD_PITCH ":"",(dds.header.dwFlags&DDSD_PIXELFORMAT)?"DDSD_PIXELFORMAT ":"",(dds.header.dwFlags&DDSD_MIPMAPCOUNT)?"DDSD_MIPMAPCOUNT ":"",(dds.header.dwFlags&DDSD_LINEARSIZE)?"DDSD_LINEARSIZE ":"",(dds.header.dwFlags&DDSD_DEPTH)?"DDSD_DEPTH":""));
    FXTRACE((100,"dds.header.dwHeight=%d\n",dds.header.dwHeight));
    FXTRACE((100,"dds.header.dwWidth=%d\n",dds.header.dwWidth));
    FXTRACE((100,"dds.header.dwDepth=%d\n",dds.header.dwDepth));
    FXTRACE((100,"dds.header.dwLinearSize=%d\n",dds.header.dwLinearSize));
    FXTRACE((100,"dds.header.dwMipMapCount=%d\n",dds.header.dwMipMapCount));
    FXTRACE((100,"dds.header.ddpf.dwSize=%d\n",dds.header.ddpf.dwSize));
    FXTRACE((100,"dds.header.ddpf.dwFlags=0x%08x: %s%s%s%s%s%s%s%s%s%s%s\n",dds.header.ddpf.dwFlags,(dds.header.ddpf.dwFlags&DDPF_ALPHAPIXELS)?"DDPF_ALPHAPIXELS ":"",(dds.header.ddpf.dwFlags&DDPF_ALPHA)?"DDPF_ALPHA ":"",(dds.header.ddpf.dwFlags&DDPF_FOURCC)?"DDPF_FOURCC ":"",(dds.header.ddpf.dwFlags&DDPF_RGB)?"DDPF_RGB ":"",(dds.header.ddpf.dwFlags&DDPF_PALETTEINDEXED1)?"DDPF_PALETTEINDEXED1 ":"",(dds.header.ddpf.dwFlags&DDPF_PALETTEINDEXED2)?"DDPF_PALETTEINDEXED2 ":"",(dds.header.ddpf.dwFlags&DDPF_PALETTEINDEXED4)?"DDPF_PALETTEINDEXED4 ":"",(dds.header.ddpf.dwFlags&DDPF_PALETTEINDEXED8)?"DDPF_PALETTEINDEXED8 ":"",(dds.header.ddpf.dwFlags&DDPF_LUMINANCE)?"DDPF_LUMINANCE ":"",(dds.header.ddpf.dwFlags&DDPF_ALPHAPREMULT)?"DDPF_ALPHAPREMULT ":"",(dds.header.ddpf.dwFlags&DDPF_NORMAL)?"DDPF_NORMAL":""));
    FXTRACE((100,"dds.header.ddpf.dwFourCC=0x%08x (%d) (%-4s)\n",dds.header.ddpf.dwFourCC,dds.header.ddpf.dwFourCC,(FXchar*)&dds.header.ddpf.dwFourCC));
    FXTRACE((100,"dds.header.ddpf.dwRGBBitCount=%d\n",dds.header.ddpf.dwRGBBitCount));
    FXTRACE((100,"dds.header.ddpf.dwRBitMask=0x%08x\n",dds.header.ddpf.dwRBitMask));
    FXTRACE((100,"dds.header.ddpf.dwGBitMask=0x%08x\n",dds.header.ddpf.dwGBitMask));
    FXTRACE((100,"dds.header.ddpf.dwBBitMask=0x%08x\n",dds.header.ddpf.dwBBitMask));
    FXTRACE((100,"dds.header.ddpf.dwABitMask=0x%08x\n",dds.header.ddpf.dwABitMask));

    FXTRACE((100,"dds.header.dwCaps =0x%08x: %s%s%s\n",dds.header.dwCaps,(dds.header.dwCaps&DDSCAPS_COMPLEX)?"DDSCAPS_COMPLEX ":"",(dds.header.dwCaps&DDSCAPS_TEXTURE)?"DDSCAPS_TEXTURE ":"",(dds.header.dwCaps&DDSCAPS_MIPMAP)?"DDSCAPS_MIPMAP":""));
    FXTRACE((100,"dds.header.dwCaps2=0x%08x: %s%s%s%s%s%s%s%s\n",dds.header.dwCaps2,(dds.header.dwCaps2&DDSCAPS2_CUBEMAP)?"DDSCAPS2_CUBEMAP ":"",(dds.header.dwCaps2&DDSCAPS2_VOLUME)?"DDSCAPS2_VOLUME ":"",(dds.header.dwCaps2&DDSCAPS2_CUBEMAP_POSITIVEX)?"DDSCAPS2_CUBEMAP_POSITIVEX ":"",(dds.header.dwCaps2&DDSCAPS2_CUBEMAP_NEGATIVEX)?"DDSCAPS2_CUBEMAP_NEGATIVEX ":"",(dds.header.dwCaps2&DDSCAPS2_CUBEMAP_POSITIVEY)?"DDSCAPS2_CUBEMAP_POSITIVEY ":"",(dds.header.dwCaps2&DDSCAPS2_CUBEMAP_NEGATIVEY)?"DDSCAPS2_CUBEMAP_NEGATIVEY ":"",(dds.header.dwCaps2&DDSCAPS2_CUBEMAP_POSITIVEZ)?"DDSCAPS2_CUBEMAP_POSITIVEZ ":"",(dds.header.dwCaps2&DDSCAPS2_CUBEMAP_NEGATIVEZ)?"DDSCAPS2_CUBEMAP_NEGATIVEZ ":""));
    FXTRACE((100,"dds.header.dwCaps3=0x%08x\n",dds.header.dwCaps3));
    FXTRACE((100,"dds.header.dwCaps4=0x%08x\n",dds.header.dwCaps4));

    FXTRACE((100,"dds.xheader.dxgiFormat=%d\n",dds.xheader.dxgiFormat));
    FXTRACE((100,"dds.xheader.resourceDimension=%d\n",dds.xheader.resourceDimension));
    FXTRACE((100,"dds.xheader.miscFlag=%d\n",dds.xheader.miscFlag));
    FXTRACE((100,"dds.xheader.arraySize=%d\n",dds.xheader.arraySize));

    // Fix depth
    if(!(dds.header.dwFlags&DDSD_DEPTH) || (dds.header.dwDepth==0)) dds.header.dwDepth=1;

    // Fix mipmap count
    if(!(dds.header.dwFlags&DDSD_MIPMAPCOUNT) || (dds.header.dwMipMapCount==0)) dds.header.dwMipMapCount=1;

    // Set image size to return
    width=dds.header.dwWidth;
    height=dds.header.dwHeight;
    depth=dds.header.dwDepth;

    // Perhaps broken format; assume DDPF_FOURCC
    if(dds.header.ddpf.dwFlags==0 && dds.header.ddpf.dwFourCC!=0){
      dds.header.ddpf.dwFlags=DDPF_FOURCC;
      }

    // Figure out how much to allocate for compressed data
    if(dds.header.ddpf.dwFlags&DDPF_FOURCC){
      switch(dds.header.ddpf.dwFourCC){
        case FOURCC_DXT1:
        case FOURCC_ATI1:
          blocksize=((width+3)>>2)*((height+3)>>2)*depth*8;
          break;
        case FOURCC_DXT2:
        case FOURCC_DXT3:
        case FOURCC_DXT4:
        case FOURCC_DXT5:
        case FOURCC_ATI2:
        case FOURCC_RXGB:
          blocksize=((width+3)>>2)*((height+3)>>2)*depth*16;
          break;
        case FOURCC_R16F:
          blocksize=width*height*depth*2;
          break;
        case FOURCC_G16R16F:
          blocksize=width*height*depth*4;
          break;
        case FOURCC_A16B16G16R16F:
          blocksize=width*height*depth*8;
          break;
        case FOURCC_R32F:
          blocksize=width*height*depth*4;
          break;
        case FOURCC_G32R32F:
          blocksize=width*height*depth*8;
          break;
        case FOURCC_A32B32G32R32F:
          blocksize=width*height*depth*16;
          break;
        default:
          goto x;       // Unsupported compression code
        }
      }

    // Figure out how much to allocate for RGB
    else if(dds.header.ddpf.dwFlags&DDPF_RGB){
      blocksize=width*height*depth*dds.header.ddpf.dwRGBBitCount/8;
      }

    // Luminance
    else if(dds.header.ddpf.dwFlags&DDPF_LUMINANCE){
      blocksize=width*height*depth*dds.header.ddpf.dwRGBBitCount/8;
      }

    // Unsupported format
    else{
      goto x;           // Not supported
      }

    // Allocate array for compressed data
    if(allocElms(dds.data,blocksize)){

      // Allocate output image
      if(allocElms(data,width*height*depth)){

        // Load temp array
        store.load(dds.data,blocksize);

        // FOURCC format
        if(dds.header.ddpf.dwFlags&DDPF_FOURCC){
          switch(dds.header.ddpf.dwFourCC){
            case FOURCC_DXT1:
              ok=dds_decompress_DXT1(dds,(FXuchar*)data);
              break;
            case FOURCC_DXT2:
              ok=dds_decompress_DXT2(dds,(FXuchar*)data);
              break;
            case FOURCC_DXT3:
              ok=dds_decompress_DXT3(dds,(FXuchar*)data);
              break;
            case FOURCC_DXT4:
              ok=dds_decompress_DXT4(dds,(FXuchar*)data);
              break;
            case FOURCC_DXT5:
              ok=dds_decompress_DXT5(dds,(FXuchar*)data);
              break;
            case FOURCC_ATI1:
              ok=dds_decompress_BC4(dds,(FXuchar*)data);
              break;
            case FOURCC_ATI2:
              ok=dds_decompress_3DC(dds,(FXuchar*)data);
              break;
            case FOURCC_RXGB:
              ok=dds_decompress_RXGB(dds,(FXuchar*)data);
              break;
            case FOURCC_R16F:
              ok=dds_decompress_R16F(dds,(FXuchar*)data);
              break;
            case FOURCC_G16R16F:
              ok=dds_decompress_G16R16F(dds,(FXuchar*)data);
              break;
            case FOURCC_A16B16G16R16F:
              ok=dds_decompress_A16B16G16R16F(dds,(FXuchar*)data);
              break;
            case FOURCC_R32F:
              ok=dds_decompress_R32F(dds,(FXuchar*)data);
              break;
            case FOURCC_G32R32F:
              ok=dds_decompress_G32R32F(dds,(FXuchar*)data);
              break;
            case FOURCC_A32B32G32R32F:
              ok=dds_decompress_A32B32G32R32F(dds,(FXuchar*)data);
              break;
            }
          }

        // RGB format
        else if(dds.header.ddpf.dwFlags&DDPF_RGB){
          ok=dds_decompress_RGB(dds,(FXuchar*)data);
          }

        // Lumimance format
        else{
          ok=dds_decompress_LUM(dds,(FXuchar*)data);
          }
        }

      // Free temp array of encoded pixels
      freeElms(dds.data);
      }
    }

  // Restore original byte orientation
x:store.swapBytes(swap);

  // Done
  return ok;
  }


// Save a dds file to a stream
FXbool fxsaveDDS(FXStream& store,FXColor* data,FXint width,FXint height,FXint depth){
  DDSImage dds;
  FXbool swap;

  // Must make sense
  if(!data || width<=0 || height<=0 || depth<=0) return false;

  // Switch byte order for the duration
  swap=store.swapBytes();
  store.setBigEndian(false);

  // Initialize header
  dds.magic=DDSD_MAGIC;
  dds.header.dwSize=sizeof(DDSHeader);
  dds.header.dwFlags=DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT|DDSD_MIPMAPCOUNT|DDSD_LINEARSIZE;
  dds.header.dwHeight=height;
  dds.header.dwWidth=width;
  dds.header.dwLinearSize=width*height*depth*4;
  dds.header.dwDepth=depth;
  dds.header.dwMipMapCount=1;
  memset(dds.header.dwReserved1,0,sizeof(dds.header.dwReserved1));
  dds.header.ddpf.dwSize=sizeof(DDSPixelFormat);
  dds.header.ddpf.dwFlags=DDPF_RGB;
  dds.header.ddpf.dwFourCC=0;
  dds.header.ddpf.dwRGBBitCount=32;
  dds.header.ddpf.dwRBitMask=0x000000ff;
  dds.header.ddpf.dwGBitMask=0x0000ff00;
  dds.header.ddpf.dwBBitMask=0x00ff0000;
  dds.header.ddpf.dwABitMask=0xff000000;
  if(1<depth){
    dds.header.dwCaps=DDSCAPS_COMPLEX|DDSCAPS_TEXTURE;
    dds.header.dwCaps2=DDSCAPS2_VOLUME;
    }
  else{
    dds.header.dwCaps=DDSCAPS_TEXTURE;
    dds.header.dwCaps2=0;
    }
  dds.header.dwCaps3=0;
  dds.header.dwCaps4=0;
  dds.header.dwReserved2=0;

  // Start saving now
  store << dds.magic;
  store << dds.header.dwSize;
  store << dds.header.dwFlags;
  store << dds.header.dwHeight;
  store << dds.header.dwWidth;
  store << dds.header.dwLinearSize;
  store << dds.header.dwDepth;
  store << dds.header.dwMipMapCount;
  store.save(dds.header.dwReserved1,11);
  store << dds.header.ddpf.dwSize;
  store << dds.header.ddpf.dwFlags;
  store << dds.header.ddpf.dwFourCC;
  store << dds.header.ddpf.dwRGBBitCount;
  store << dds.header.ddpf.dwRBitMask;
  store << dds.header.ddpf.dwGBitMask;
  store << dds.header.ddpf.dwBBitMask;
  store << dds.header.ddpf.dwABitMask;
  store << dds.header.dwCaps;
  store << dds.header.dwCaps2;
  store << dds.header.dwCaps3;
  store << dds.header.dwCaps4;
  store << dds.header.dwReserved2;

  // Data array
  store.save(data,width*height*depth);

  store.swapBytes(swap);
  return true;
  }

}
