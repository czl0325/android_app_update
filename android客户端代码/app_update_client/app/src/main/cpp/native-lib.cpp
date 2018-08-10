#include <jni.h>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
/** 导入bzip2的引用*/
#include "bzip2/bzlib.c"
#include "bzip2/crctable.c"
#include "bzip2/compress.c"
#include "bzip2/decompress.c"
#include "bzip2/randtable.c"
#include "bzip2/blocksort.c"
#include "bzip2/huffman.c"

struct bspatch_stream
{
    void* opaque;
    int (*read)(const struct bspatch_stream* stream, void* buffer, int length);
};

static int64_t offtin(uint8_t *buf)
{
    int64_t y;

    y=buf[7]&0x7F;
    y=y*256;y+=buf[6];
    y=y*256;y+=buf[5];
    y=y*256;y+=buf[4];
    y=y*256;y+=buf[3];
    y=y*256;y+=buf[2];
    y=y*256;y+=buf[1];
    y=y*256;y+=buf[0];

    if(buf[7]&0x80) y=-y;

    return y;
}

int bspatch(const uint8_t* old, int64_t oldsize, uint8_t* new1, int64_t newsize, struct bspatch_stream* stream)
{
    uint8_t buf[8];
    int64_t oldpos,newpos;
    int64_t ctrl[3];
    int64_t i;

    oldpos=0;newpos=0;
    while(newpos<newsize) {
        /* Read control data */
        for(i=0;i<=2;i++) {
            if (stream->read(stream, buf, 8))
                return -1;
            ctrl[i]=offtin(buf);
        };

        /* Sanity-check */
        if(newpos+ctrl[0]>newsize)
            return -1;

        /* Read diff string */
        if (stream->read(stream, new1 + newpos, ctrl[0]))
            return -1;

        /* Add old data to diff string */
        for(i=0;i<ctrl[0];i++)
            if((oldpos+i>=0) && (oldpos+i<oldsize))
                new1[newpos+i]+=old[oldpos+i];

        /* Adjust pointers */
        newpos+=ctrl[0];
        oldpos+=ctrl[0];

        /* Sanity-check */
        if(newpos+ctrl[1]>newsize)
            return -1;

        /* Read extra string */
        if (stream->read(stream, new1 + newpos, ctrl[1]))
            return -1;

        /* Adjust pointers */
        newpos+=ctrl[1];
        oldpos+=ctrl[2];
    };

    return 0;
}

static int bz2_read(const struct bspatch_stream* stream, void* buffer, int length)
{
    int n;
    int bz2err;
    BZFILE* bz2;

    bz2 = (BZFILE*)stream->opaque;
    n = BZ2_bzRead(&bz2err, bz2, buffer, length);
    if (n != length)
        return -1;

    return 0;
}

int bspatch_main(int argc,char * argv[])
{
    FILE * f, * cpf, * dpf, * epf;
    BZFILE * cpfbz2, * dpfbz2, * epfbz2;
    int cbz2err, dbz2err, ebz2err;
    int fd;
    ssize_t oldsize,newsize;
    ssize_t bzctrllen,bzdatalen;
    u_char header[32],buf[8];
    u_char *old, *new1;
    off_t oldpos,newpos;
    off_t ctrl[3];
    off_t lenread;
    off_t i;

    if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

    /* Open patch file */
    if ((f = fopen(argv[3], "r")) == NULL)
        err(1, "fopen(%s)", argv[3]);

    /*
    File format:
        0	8	"BSDIFF40"
        8	8	X
        16	8	Y
        24	8	sizeof(newfile)
        32	X	bzip2(control block)
        32+X	Y	bzip2(diff block)
        32+X+Y	???	bzip2(extra block)
    with control block a set of triples (x,y,z) meaning "add x bytes
    from oldfile to x bytes from the diff block; copy y bytes from the
    extra block; seek forwards in oldfile by z bytes".
    */

    /* Read header */
    if (fread(header, 1, 32, f) < 32) {
        if (feof(f))
            errx(1, "Corrupt patch\n");
        err(1, "fread(%s)", argv[3]);
    }

    /* Check for appropriate magic */
    if (memcmp(header, "BSDIFF40", 8) != 0)
        errx(1, "Corrupt patch\n");

    /* Read lengths from header */
    bzctrllen=offtin(header+8);
    bzdatalen=offtin(header+16);
    newsize=offtin(header+24);
    if((bzctrllen<0) || (bzdatalen<0) || (newsize<0))
        errx(1,"Corrupt patch\n");

    /* Close patch file and re-open it via libbzip2 at the right places */
    if (fclose(f))
        err(1, "fclose(%s)", argv[3]);
    if ((cpf = fopen(argv[3], "r")) == NULL)
        err(1, "fopen(%s)", argv[3]);
    if (fseeko(cpf, 32, SEEK_SET))
        err(1, "fseeko(%s, %lld)", argv[3],
            (long long)32);
    if ((cpfbz2 = BZ2_bzReadOpen(&cbz2err, cpf, 0, 0, NULL, 0)) == NULL)
        errx(1, "BZ2_bzReadOpen, bz2err = %d", cbz2err);
    if ((dpf = fopen(argv[3], "r")) == NULL)
        err(1, "fopen(%s)", argv[3]);
    if (fseeko(dpf, 32 + bzctrllen, SEEK_SET))
        err(1, "fseeko(%s, %lld)", argv[3],
            (long long)(32 + bzctrllen));
    if ((dpfbz2 = BZ2_bzReadOpen(&dbz2err, dpf, 0, 0, NULL, 0)) == NULL)
        errx(1, "BZ2_bzReadOpen, bz2err = %d", dbz2err);
    if ((epf = fopen(argv[3], "r")) == NULL)
        err(1, "fopen(%s)", argv[3]);
    if (fseeko(epf, 32 + bzctrllen + bzdatalen, SEEK_SET))
        err(1, "fseeko(%s, %lld)", argv[3],
            (long long)(32 + bzctrllen + bzdatalen));
    if ((epfbz2 = BZ2_bzReadOpen(&ebz2err, epf, 0, 0, NULL, 0)) == NULL)
        errx(1, "BZ2_bzReadOpen, bz2err = %d", ebz2err);

    if(((fd=open(argv[1],O_RDONLY,0))<0) ||
       ((oldsize=lseek(fd,0,SEEK_END))==-1) ||
       ((old=(u_char *)malloc(oldsize+1))==NULL) ||
       (lseek(fd,0,SEEK_SET)!=0) ||
       (read(fd,old,oldsize)!=oldsize) ||
       (close(fd)==-1)) err(1,"%s",argv[1]);
    if((new1=(u_char *)malloc(newsize+1))==NULL) err(1,NULL);

    oldpos=0;newpos=0;
    while(newpos<newsize) {
        /* Read control data */
        for(i=0;i<=2;i++) {
            lenread = BZ2_bzRead(&cbz2err, cpfbz2, buf, 8);
            if ((lenread < 8) || ((cbz2err != BZ_OK) &&
                                  (cbz2err != BZ_STREAM_END)))
                errx(1, "Corrupt patch\n");
            ctrl[i]=offtin(buf);
        };

        /* Sanity-check */
        if(newpos+ctrl[0]>newsize)
            errx(1,"Corrupt patch\n");

        /* Read diff string */
        lenread = BZ2_bzRead(&dbz2err, dpfbz2, new1 + newpos, ctrl[0]);
        if ((lenread < ctrl[0]) ||
            ((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END)))
            errx(1, "Corrupt patch\n");

        /* Add old data to diff string */
        for(i=0;i<ctrl[0];i++)
            if((oldpos+i>=0) && (oldpos+i<oldsize))
                new1[newpos+i]+=old[oldpos+i];

        /* Adjust pointers */
        newpos+=ctrl[0];
        oldpos+=ctrl[0];

        /* Sanity-check */
        if(newpos+ctrl[1]>newsize)
            errx(1,"Corrupt patch\n");

        /* Read extra string */
        lenread = BZ2_bzRead(&ebz2err, epfbz2, new1 + newpos, ctrl[1]);
        if ((lenread < ctrl[1]) ||
            ((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END)))
            errx(1, "Corrupt patch\n");

        /* Adjust pointers */
        newpos+=ctrl[1];
        oldpos+=ctrl[2];
    };

    /* Clean up the bzip2 reads */
    BZ2_bzReadClose(&cbz2err, cpfbz2);
    BZ2_bzReadClose(&dbz2err, dpfbz2);
    BZ2_bzReadClose(&ebz2err, epfbz2);
    if (fclose(cpf) || fclose(dpf) || fclose(epf))
        err(1, "fclose(%s)", argv[3]);

    /* Write the new file */
    if(((fd=open(argv[2],O_CREAT|O_TRUNC|O_WRONLY,0666))<0) ||
       (write(fd,new1,newsize)!=newsize) || (close(fd)==-1))
        err(1,"%s",argv[2]);

    free(new1);
    free(old);

    return 0;
}

extern "C" JNIEXPORT jstring

JNICALL
Java_com_github_app_1update_1client_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_github_app_1update_1client_MainActivity_patch(JNIEnv *env, jobject jobj, jstring oldFile_,
                                                       jstring newFile_, jstring patchFile_) {
    const char *oldFile = env->GetStringUTFChars(oldFile_, 0);
    const char *newFile = env->GetStringUTFChars(newFile_, 0);
    const char *patchFile = env->GetStringUTFChars(patchFile_, 0);

    // TODO
    int argc = 4;
    char * argv[4];
    argv[0] = static_cast<char *>((char*)"bspatch");
    argv[1] = (char*)oldFile;
    argv[2] = (char*)newFile;
    argv[3] = (char*)patchFile;
    int result = bspatch_main(argc, argv);

    env->ReleaseStringUTFChars(oldFile_, oldFile);
    env->ReleaseStringUTFChars(newFile_, newFile);
    env->ReleaseStringUTFChars(patchFile_, patchFile);

    return result;
}extern "C"
JNIEXPORT jstring JNICALL
Java_com_github_app_1update_1client_MainActivity_myTestString(JNIEnv *env, jobject jobj) {
    std::string hello = "我的测试数据";
    return env->NewStringUTF(hello.c_str());
}