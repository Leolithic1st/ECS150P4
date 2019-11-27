#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fs.h>


#define TEST_DISK_NAME              ("my_test.fs")
#define TEST_DISK_DATA_BLOCK_NUM    (200)
#define TEST_BIG_FILE_SIZE          (4096*5)
#define TEST_BIG_OFFSET             (4096*1+2048)
#define TEST_LONG_READ              (4096*2)


static void create_fs(const char* diskname, unsigned int blk_num)
{
    char tmp_cmd[200] = "";
    sprintf(tmp_cmd, "./fs_make.x %s %d", diskname, blk_num);
    if(system(tmp_cmd)); //ignore ret val
}

static void delete_fs(const char* diskname)
{
    char tmp_cmd[200] = "";
    sprintf(tmp_cmd, "rm -f %s", diskname);
    if(system(tmp_cmd)); //ignore ret val
}


void my_test_bigFile_WR(const char* diskname)
{
    printf("TEST [%s] start\n", __FUNCTION__);

    char tmp_data[TEST_BIG_FILE_SIZE] = {0};
    char tmp_rslt[TEST_BIG_FILE_SIZE] = {0};
    char tmp_char = 0;
    for(unsigned int idx = 0; idx < TEST_BIG_FILE_SIZE; ++idx){
        tmp_data[idx] = tmp_char++;
    }

    fs_mount(diskname);
    fs_create("test.dat");

    int fd = fs_open("test.dat");
    fs_write(fd, tmp_data, TEST_BIG_FILE_SIZE);
    fs_lseek(fd, 0); //put offset to 0
    fs_read(fd, tmp_rslt, TEST_BIG_FILE_SIZE);
    fs_close(fd);
    fs_umount();

    if(0 == memcmp(tmp_data, tmp_rslt, TEST_BIG_FILE_SIZE)){
        printf("TEST [%s] passed, filesize(%d)\n", __FUNCTION__, TEST_BIG_FILE_SIZE);
    }
    else{
        printf("TEST [%s] failed, filesize(%d)\n", __FUNCTION__, TEST_BIG_FILE_SIZE);
        FILE* fd_src = fopen("src.dat", "w+");
        if(fd_src){
            fwrite(tmp_data, TEST_BIG_FILE_SIZE, 1, fd_src);
        }
        FILE* fd_rslt = fopen("rslt.dat", "w+");
        if(fd_rslt){
            fwrite(tmp_rslt, TEST_BIG_FILE_SIZE, 1, fd_rslt);
        }
    }
}

void my_test_longOffset_WR(const char* diskname)
{
    printf("TEST [%s] start\n", __FUNCTION__);

    char tmp_data[TEST_BIG_FILE_SIZE] = {0};
    char tmp_rslt[TEST_LONG_READ] = {0};
    char tmp_char = 0;
    for(unsigned int idx = 0; idx < TEST_BIG_FILE_SIZE; ++idx){
        tmp_data[idx] = tmp_char++;
    }

    fs_mount(diskname);
    fs_create("test.dat");

    int fd = fs_open("test.dat");
    fs_write(fd, tmp_data, TEST_BIG_FILE_SIZE);
    fs_lseek(fd, TEST_BIG_OFFSET);
    fs_read(fd, tmp_rslt, TEST_LONG_READ);

    if(0 == memcmp(tmp_data+TEST_BIG_OFFSET, tmp_rslt, TEST_LONG_READ)){
        printf("TEST [%s] long read passed, size(%d)\n", __FUNCTION__, TEST_LONG_READ);
    }
    else{
        printf("TEST [%s] long read failed, size(%d)\n", __FUNCTION__, TEST_LONG_READ);
        fs_close(fd);
        fs_umount();
        return;
    }

    //update data
    for(unsigned int idx = 0; idx < TEST_LONG_READ; ++idx){
        tmp_char += 2;
        tmp_data[idx+TEST_BIG_OFFSET] = tmp_char;
    }

    fs_lseek(fd, TEST_BIG_OFFSET);
    fs_write(fd, tmp_data+TEST_BIG_OFFSET, TEST_LONG_READ);
    fs_lseek(fd, TEST_BIG_OFFSET);
    fs_read(fd, tmp_rslt, TEST_LONG_READ);

    if(0 == memcmp(tmp_data+TEST_BIG_OFFSET, tmp_rslt, TEST_LONG_READ)){
        printf("TEST [%s] long write passed, size(%d)\n", __FUNCTION__, TEST_LONG_READ);
    }
    else{
        printf("TEST [%s] long write failed, size(%d)\n", __FUNCTION__, TEST_LONG_READ);
    }

    fs_close(fd);
    fs_umount();
}

void my_test_fullFiles(const char* diskname)
{
    printf("TEST [%s] start\n", __FUNCTION__);

    fs_mount(diskname);

    char tmp_name[30] = {0};
    for(unsigned int cnt = 0; cnt < FS_FILE_MAX_COUNT; cnt++){
        sprintf(tmp_name, "test_%d.dat", cnt);
        if(-1 == fs_create(tmp_name)){
            printf("TEST [%s] failed, create file failed,cnt(%d)\n", __FUNCTION__, cnt);
            fs_umount();
            return;
        }
    }

    if(-1 != fs_create("lastfile.log")){
        printf("TEST [%s] failed, created more than %d files\n", __FUNCTION__, FS_FILE_MAX_COUNT);
        fs_umount();
        return;
    }

    if(-1 == fs_delete("test_0.dat")){
        printf("TEST [%s] failed, delete file failed\n", __FUNCTION__);
        fs_umount();
        return;
    }

    if(-1 == fs_create("lastfile.log")){
        printf("TEST [%s] failed, create file failed\n", __FUNCTION__);
        fs_umount();
        return;
    }

    fs_umount();
    printf("TEST [%s] passed, created files cnt(%d)\n", __FUNCTION__, FS_FILE_MAX_COUNT);
    return;
}

void my_test_fullOpenedFiles(const char* diskname)
{
    printf("TEST [%s] start\n", __FUNCTION__);

    char* tmp_data = "qwertyuiopasdfghjl1234567890";

    fs_mount(diskname);
    fs_create("test.dat");
    int fd = fs_open("test.dat");
    fs_write(fd, tmp_data, strlen(tmp_data));
    fs_close(fd);
    int fd_array[FS_OPEN_MAX_COUNT] = {0};

    for(unsigned int cnt = 0; cnt < FS_OPEN_MAX_COUNT; ++cnt){
        fd_array[cnt] = fs_open("test.dat");
        if(-1 == fd_array[cnt]){
            printf("TEST [%s] failed, open file failed, cnt(%d)\n", __FUNCTION__, cnt);
            fs_umount();
            return;
        }
    }

    int tmp_fd = 0;
    tmp_fd = fs_open("test.dat");
    if(-1 != tmp_fd){
        printf("TEST [%s] failed, opened more than %d files\n", __FUNCTION__, FS_OPEN_MAX_COUNT);
        fs_umount();
        return;
    }

    if(-1 == fs_close(fd_array[0])){
        printf("TEST [%s] failed, close file failed\n", __FUNCTION__);
        fs_umount();
        return;
    }

    fd_array[0] = fs_open("test.dat");
    if(-1 == fd_array[0]){
        printf("TEST [%s] failed, open file failed\n", __FUNCTION__);
        fs_umount();
        return;
    }

    //double check file status
    int fileSize_L = 0;
    int fileSize_R = 0;
    for(unsigned int idx = 0; idx < FS_OPEN_MAX_COUNT-1; ++idx){
        fileSize_L = fs_stat(fd_array[idx]);
        fileSize_R = fs_stat(fd_array[idx+1]);
        if(fileSize_L != fileSize_R){
            printf("TEST [%s] failed, file size mismatch(%d)!=(%d)\n",
                __FUNCTION__, fileSize_L, fileSize_R);
                fs_umount();
            return;
        }
    }

    for(unsigned int idx = 0; idx < FS_OPEN_MAX_COUNT-1; ++idx){
        if(fd_array[idx] == fd_array[idx+1]){
            printf("TEST [%s] failed, file descriptor equals\n", __FUNCTION__);
            fs_umount();
            return;
        }
    }

    fs_umount();
    printf("TEST [%s] passed, opened files cnt(%d)\n", __FUNCTION__, FS_OPEN_MAX_COUNT);
    return;
}

int main(int argc, char **argv)
{
    delete_fs(TEST_DISK_NAME);

    create_fs(TEST_DISK_NAME, TEST_DISK_DATA_BLOCK_NUM);
    my_test_bigFile_WR(TEST_DISK_NAME);
    delete_fs(TEST_DISK_NAME);

    create_fs(TEST_DISK_NAME, TEST_DISK_DATA_BLOCK_NUM);
    my_test_longOffset_WR(TEST_DISK_NAME);
    delete_fs(TEST_DISK_NAME);

    create_fs(TEST_DISK_NAME, TEST_DISK_DATA_BLOCK_NUM);
    my_test_fullFiles(TEST_DISK_NAME);
    delete_fs(TEST_DISK_NAME);

    create_fs(TEST_DISK_NAME, TEST_DISK_DATA_BLOCK_NUM);
    my_test_fullOpenedFiles(TEST_DISK_NAME);
    delete_fs(TEST_DISK_NAME);

    return 0;
}
