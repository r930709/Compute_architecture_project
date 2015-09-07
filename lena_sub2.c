/*--------------final project rgb to gray sacle app code */
/*-------1. must care what header file we need
---------2. we need three address to put three picture(original_rgb,gray_scale_old,gray_sacle_make)
---------3. add those function to communicate with driver code open(),write(),read(),ioctl()
*/
#include <stdio.h>
#include <stdlib.h>
//----------------------//
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

/*  command  */
#define OP_SET 1
#define GO	   5

#define START  1
#define ADD 1
#define SUB 2
#define MUL 3
#define DIV 4

 int upside_down(const char *fname_s, const char *fname_ss, const char *fname_t) {
   FILE          *fp_s = NULL;    // source file handler
   FILE          *fp_ss = NULL;    // source2 file handler
    FILE          *fp_t = NULL;    // target file handler
    unsigned int  x,y,count;             // for loop counter
    unsigned int width, height;   // image width, image height
    unsigned char *image_s = NULL; // source image array
    unsigned char *image_ss = NULL; // source2 image array
    unsigned char *image_t = NULL; // target image array
    unsigned char R, G, B;         // color of R, G, B
    unsigned char R1, G1, B1;         // color of R, G, B
    unsigned char R2, G2, B2;         // color of R, G, B
    unsigned char Rs, Gs, Bs;         // color of R, G, B
    unsigned char RGBsum = 0;         // color of R, G, B

   unsigned char header[54] = {
      0x42,        // identity : B
      0x4d,        // identity : M
      0, 0, 0, 0,  // file size
      0, 0,        // reserved1
      0, 0,        // reserved2
     54, 0, 0, 0, // RGB data offset
      40, 0, 0, 0, // struct BITMAPINFOHEADER size
      0, 0, 0, 0,  // bmp width
      0, 0, 0, 0,  // bmp height
      1, 0,        // planes
     24, 0,       // bit per pixel
      0, 0, 0, 0,  // compression
      0, 0, 0, 0,  // data size
      0, 0, 0, 0,  // h resolution
      0, 0, 0, 0,  // v resolution
      0, 0, 0, 0,  // used colors
     0, 0, 0, 0   // important colors
    };

    unsigned int file_size;           // file size
    unsigned int rgb_raw_data_offset; // RGB raw data offset

    //讀檔案
    fp_s = fopen(fname_s, "rb");
    if (fp_s == NULL) {
      printf("fopen fp_s error\n");
      return -1;
    }

    // move offset to 10 to find rgb raw data offset
    fseek(fp_s, 10, SEEK_SET);
    fread(&rgb_raw_data_offset, sizeof(unsigned int), 1, fp_s);

    // move offset to 18    to get width & height;
    fseek(fp_s, 18, SEEK_SET);
    fread(&width,  sizeof(unsigned int), 1, fp_s);
    fread(&height, sizeof(unsigned int), 1, fp_s);

    // move offset to rgb_raw_data_offset to get RGB raw data
    fseek(fp_s, rgb_raw_data_offset, SEEK_SET);

    image_s = (unsigned char *)malloc((size_t)width * height * 3);

    if (image_s == NULL) {
     printf("malloc images_s error\n");
      return -1;
    }

   image_t = (unsigned char *)malloc((size_t)width * height * 3);


    if (image_t == NULL) {
      printf("malloc image_t error\n");
     return -1;
   }

   int fin=0;
   int operand[3];
   int rgbnew[1];

   fin = open("/dev/lenadriver", O_RDWR);
   if(fin==0) {
		perror("Error opening device\n");
		exit(1);
	}

//------------------------------------------------------------------------------------------//
    fread(image_s, sizeof(unsigned char), (size_t)(long)width * height * 3, fp_s);

    for(y = 0; y != height; ++y) {
     for(x = 0; x != width; ++x) {
       R = *(image_s + 3 * (width * y + x) + 2);
       G = *(image_s + 3 * (width * y + x) + 1);
       B = *(image_s + 3 * (width * y + x) + 0);


    operand[0] = R;
	operand[1] = G;
    operand[2] = B;


    write(fin, operand, 12);
    ioctl(fin, GO, START);
    read(fin, rgbnew, 4);

       *(image_t + 3 * (width * y + x) + 2) = rgbnew[0];
       *(image_t + 3 * (width * y + x) + 1) = rgbnew[0];
       *(image_t + 3 * (width * y + x) + 0) = rgbnew[0];

     }
   }

    close(fin);

    // write to new bmp
   fp_t = fopen(fname_t, "wb");
  if (fp_t == NULL) {
     printf("fopen fname_t error\n");
        return -1;
      }

     // file size
     file_size = width * height * 3 + rgb_raw_data_offset;
     header[2] = (unsigned char)(file_size & 0x000000ff);
     header[3] = (file_size >> 8)  & 0x000000ff;
     header[4] = (file_size >> 16) & 0x000000ff;
     header[5] = (file_size >> 24) & 0x000000ff;

     // width
     header[18] = width & 0x000000ff;
     header[19] = (width >> 8)  & 0x000000ff;
     header[20] = (width >> 16) & 0x000000ff;
     header[21] = (width >> 24) & 0x000000ff;

     // height
     header[22] = height &0x000000ff;
     header[23] = (height >> 8)  & 0x000000ff;
     header[24] = (height >> 16) & 0x000000ff;
     header[25] = (height >> 24) & 0x000000ff;

   		// write header
   		fwrite(header, sizeof(unsigned char), rgb_raw_data_offset, fp_t);
   		// write image
		fwrite(image_t, sizeof(unsigned char), (size_t)(long)width * height * 3, fp_t);

     //讀第二個圖
	unsigned char header2[54] = {
      0x42,        // identity : B
      0x4d,        // identity : M
      0, 0, 0, 0,  // file size
      0, 0,        // reserved1
      0, 0,        // reserved2
     54, 0, 0, 0, // RGB data offset
      40, 0, 0, 0, // struct BITMAPINFOHEADER size
      0, 0, 0, 0,  // bmp width
      0, 0, 0, 0,  // bmp height
      1, 0,        // planes
     24, 0,       // bit per pixel
      0, 0, 0, 0,  // compression
      0, 0, 0, 0,  // data size
      0, 0, 0, 0,  // h resolution
      0, 0, 0, 0,  // v resolution
      0, 0, 0, 0,  // used colors
     0, 0, 0, 0   // important colors
    };

    unsigned int file_size2;           // file size
    unsigned int rgb_raw_data_offset2; // RGB raw data offset

    //讀檔案2
    fp_ss = fopen(fname_ss, "rb");
    if (fp_ss == NULL) {
      printf("fopen fp_ss error\n");
      return -1;
    }

    // move offset to 10 to find rgb raw data offset
    fseek(fp_ss, 10, SEEK_SET);
    fread(&rgb_raw_data_offset2, sizeof(unsigned int), 1, fp_ss);
    // move offset to 18    to get width & height;
    fseek(fp_ss, 18, SEEK_SET);
    fread(&width,  sizeof(unsigned int), 1, fp_ss);
    fread(&height, sizeof(unsigned int), 1, fp_ss);
    // move offset to rgb_raw_data_offset to get RGB raw data
    fseek(fp_ss, rgb_raw_data_offset2, SEEK_SET);

   image_ss = (unsigned char *)malloc((size_t)width * height * 3);
    if (image_ss == NULL) {
      printf("malloc image_ss error\n");
     return -1;
   }

    fread(image_ss, sizeof(unsigned char), (size_t)(long)width * height * 3, fp_ss);


	// 相減

   for(y = 0; y != height; ++y) {
     for(x = 0; x != width; ++x) {
       R1 = *(image_ss + 3 * (width * y + x) + 2);
       G1 = *(image_ss + 3 * (width * y + x) + 1);
       B1 = *(image_ss + 3 * (width * y + x) + 0);

       R2 = *(image_t + 3 * (width * y + x) + 2);
       G2 = *(image_t + 3 * (width * y + x) + 1);
    	B2 = *(image_t + 3 * (width * y + x) + 0);

    	Rs = R1 - R2;
    	Gs = G1 - G2;
    	Bs = B1 - B2;

    	RGBsum = RGBsum + Rs + Gs + Bs;

     }
   }

     fclose(fp_s);
     fclose(fp_ss);
     fclose(fp_t);

     printf("The result of the subtraction:");
     printf("%d\n", RGBsum);

    return 0;


 }

 int main(void) {
   upside_down("lena.bmp", "lena2.bmp", "lena3.bmp");

 }
