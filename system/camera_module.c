#include <stdio.h>
#include <stdlib.h>  // Include this header for EXIT_SUCCESS and EXIT_FAILURE
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <jpeglib.h>

#define DEVICE_NAME "/dev/video0"
#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 480
#define JPEG_QUALITY 75

struct buffer {
    void *start;
    size_t length;
};

static struct buffer *buffers;
static int num_buffers;

void save_jpeg(const char *filename, unsigned char *image_buffer, int width, int height) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    FILE *outfile; /* target file */
    JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
    int row_stride; /* physical row width in image buffer */

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "cannot open %s\n", filename);
        exit(1);
    }
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width; /* image width and height, in pixels */
    cinfo.image_height = height;
    cinfo.input_components = 3; /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; /* colorspace of input image */

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = width * 3; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &image_buffer[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
}

int capture_image(const char *filename) {
    int fd = open(DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("Opening video device");
        return EXIT_FAILURE;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = IMAGE_WIDTH;
    fmt.fmt.pix.height = IMAGE_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // Use RGB format
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("Setting Pixel Format");
        close(fd);
        return EXIT_FAILURE;
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("Requesting Buffer");
        close(fd);
        return EXIT_FAILURE;
    }

    buffers = calloc(req.count, sizeof(*buffers));
    for (num_buffers = 0; num_buffers < req.count; ++num_buffers) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = num_buffers;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("Querying Buffer");
            close(fd);
            return EXIT_FAILURE;
        }

        buffers[num_buffers].length = buf.length;
        buffers[num_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

// Queue the buffer
if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
    perror("Queueing Buffer");
    close(fd);
    return EXIT_FAILURE;
}

// Start streaming before dequeuing
int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
    perror("Starting Streaming");
    close(fd);
    return EXIT_FAILURE;
}

// Dequeue the buffer
if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
    perror("Dequeueing Buffer");
    close(fd);
    return EXIT_FAILURE;
}

    // Save the image as JPEG
    save_jpeg(filename, buffers[0].start, IMAGE_WIDTH, IMAGE_HEIGHT);

    for (int i = 0; i < num_buffers; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }
    free(buffers);
    close(fd);
    return EXIT_SUCCESS;
}
