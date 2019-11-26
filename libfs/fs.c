#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"


#pragma pack(1)


#define my_min(x,y)     ( ((x)>=(y))?y:x )
#define FAT_EOC         (0xFFFF)
#define DEFAULT_SIGN    ("ECS150FS")

typedef struct _super_block_info_s_{
    char sign[8];
    uint16_t block_num_total;
    uint16_t root_dir_block_idx;
    uint16_t data_block_idx;
    uint16_t data_block_num;
    uint8_t fat_block_num;
    uint8_t reserve[4079];
}Super_Block_Info;

typedef struct _FAT_info_s_{
    uint16_t* data;
}FAT_Info;

typedef struct _file_entry_s_{
    char filename[FS_FILENAME_LEN];
    uint32_t file_size;
    uint16_t start_data_block_idx;
    uint8_t reserve[10];
}File_Entry;

typedef struct _root_dir_info_s_{
    File_Entry files[FS_FILE_MAX_COUNT];
}Root_Dir_Info;

typedef struct _data_block_s_{
    uint8_t data[BLOCK_SIZE];
}Data_Block;

typedef struct _all_data_block_s_{
    Data_Block** data_all;
}All_Data_Block;

typedef struct _file_des_s_{
    uint16_t idx;
    uint32_t offset;
}File_Des;


static Super_Block_Info g_superBlockInfo = {0};
static uint32_t g_FATLen = 0;
static FAT_Info g_FATInfo = {0};
static Root_Dir_Info g_rootDirInfo = {0};
static All_Data_Block g_all_data = {0};
static uint16_t g_fileNumTotal = 0;
static File_Des* g_openedFiles[FS_OPEN_MAX_COUNT] = {0};
static uint16_t g_openedFileNum = 0;


static uint16_t _get_fs_file_num(void)
{
    uint16_t cnt = 0;
    for(uint16_t idx = 0; idx < FS_FILE_MAX_COUNT; ++idx){
        if(0 != g_rootDirInfo.files[idx].start_data_block_idx){
            cnt++;
        }
    }

    return cnt;
}

static uint16_t _get_free_FAT_num(void)
{
    uint16_t cnt = 0;
    for(uint16_t idx = 0; idx < g_FATLen; ++idx){
        if(0 == g_FATInfo.data[idx]){
            cnt++;
        }
    }

    return cnt;
}

static int16_t _find_openedFile_by_fd(File_Des* fd)
{
    for(uint16_t idx = 0; idx < FS_FILE_MAX_COUNT; ++idx){
        if(fd == g_openedFiles[idx]){
            return idx;
        }
    }
    return -1;
}

static int16_t _find_openedFile_by_name(const char* filename)
{
    File_Des* fd = NULL;
    for(uint16_t idx = 0; idx < FS_FILE_MAX_COUNT; ++idx){
        fd = g_openedFiles[idx];
        if(NULL != fd){
            if(0 == strncmp(filename, (g_rootDirInfo.files[fd->idx]).filename, strlen(filename))){
                return idx;
            }
        }
    }
    return -1;
}

static int16_t _find_space_for_open_file(void)
{
    return _find_openedFile_by_fd(NULL);
}

static int16_t _search_file_by_filename(const char* filename)
{
    for(uint16_t idx = 0; idx < FS_FILE_MAX_COUNT; ++idx){
        if( 0 == strncmp(filename, g_rootDirInfo.files[idx].filename, strlen(filename)) ){
            return idx;
        }
    }

    //not found
    return -1;
}

static int8_t _check_filename(const char* filename)
{
    if(NULL == filename){
        return -1;
    }

    uint16_t name_len = strlen(filename);
    if(FILENAME_MAX < name_len){
        return -1;
    }

    return 0;
}

static int16_t _find_empty_entry(void)
{
    for(uint16_t idx = 0; idx < FS_FILE_MAX_COUNT; ++idx){
        if(0 == g_rootDirInfo.files[idx].start_data_block_idx){
            return idx;
        }
    }

    return -1;
}

static uint16_t _get_block_idx_for_new_pos(uint16_t file_idx, uint32_t start_pos, uint32_t step_len)
{
    uint32_t len_total = start_pos+step_len;
    uint16_t offset_to_block_num = len_total/BLOCK_SIZE;
    uint16_t block_idx = g_rootDirInfo.files[file_idx].start_data_block_idx;

    for(uint16_t cnt = 0; cnt < offset_to_block_num; ++cnt){
        if(FAT_EOC != g_FATInfo.data[block_idx]){
            block_idx = g_FATInfo.data[block_idx];
        }
    }

    return block_idx;
}

static int32_t _find_empty_FAT(void)
{
    for(uint32_t idx = 1; idx < g_FATLen; ++idx){
        if(0 == g_FATInfo.data[idx]){
            return idx;
        }
    }

    return -1;
}



/////////////////////API
int fs_mount(const char *diskname)
{
//    printf("%s\n", __FUNCTION__);
    int mnt_ret = block_disk_open(diskname);
    if (0 != mnt_ret)
    {
        return mnt_ret;
    }
    else
    {
        //mount fs
        //read super block
        if(-1 == block_read(0, &g_superBlockInfo)){
            return -1;
        }

        //check signature
        if(0 != strncmp(DEFAULT_SIGN, g_superBlockInfo.sign, strlen(DEFAULT_SIGN))){
            return -1;
        }

        //invalidate amount of block
        if(g_superBlockInfo.block_num_total != block_disk_count()){
            return -1;
        }

        //read fat
        g_FATLen = g_superBlockInfo.data_block_num;
        //real len should be g_FATLen
        //but easy for reading or writing, malloc max len of memory
        g_FATInfo.data = (uint16_t*)malloc(BLOCK_SIZE*g_superBlockInfo.fat_block_num);
        if(NULL == g_FATInfo.data){
            return -1;
        }

        uint32_t read_cnt = 0;
        for(uint8_t cnt = 0; cnt < g_superBlockInfo.fat_block_num; ++cnt){
            if( -1 == block_read(1+cnt, g_FATInfo.data+read_cnt) ){
                return -1;
            }
            read_cnt += BLOCK_SIZE;
        }

#if 0
        for(uint32_t idx = 0; idx < g_FATLen; idx++){
            if(0 != g_FATInfo.data[idx]){
                printf("idx(%d),val(%d)\n", idx, g_FATInfo.data[idx]);
            }
        }
#endif

        //read root dir info
        if( -1 == block_read(g_superBlockInfo.fat_block_num+1, &g_rootDirInfo) ){
            return -1;
        }

#if 0
        for(uint16_t idx = 0; idx < FS_FILE_MAX_COUNT; idx++){
            if(0 != g_rootDirInfo.files[idx].start_data_block_idx){
                printf("[%d]filename(%s),size(%d),idx(%d)\n", idx,
                    g_rootDirInfo.files[idx].filename,
                    g_rootDirInfo.files[idx].file_size,
                    g_rootDirInfo.files[idx].start_data_block_idx);
            }
        }
#endif

        //read all data
        g_all_data.data_all = (Data_Block**)malloc(sizeof(Data_Block*)*g_superBlockInfo.data_block_num);
        memset(g_all_data.data_all, 0, sizeof(Data_Block*)*g_superBlockInfo.data_block_num);
        for(uint16_t cnt = 0; cnt < g_superBlockInfo.data_block_num; ++cnt){
            Data_Block* tmp_db = (Data_Block*)malloc(sizeof(Data_Block));
            memset(tmp_db, 0, sizeof(Data_Block));
            if( -1 == block_read(g_superBlockInfo.data_block_idx+cnt, tmp_db) ){
                return -1;
            }

            g_all_data.data_all[cnt] = tmp_db;
        }

        g_fileNumTotal = _get_fs_file_num();
    }

    return 0;
}

int fs_umount(void)
{
//    printf("%s\n", __FUNCTION__);
    //check?

    //write super block
    if( -1 == block_write(0, &g_superBlockInfo) ){
        return -1;
    }

    //write fat
    const uint16_t block_step = BLOCK_SIZE/sizeof(uint16_t);
    for(uint8_t cnt = 0; cnt < g_superBlockInfo.fat_block_num; ++cnt){
        if( -1 == block_write(1+cnt, g_FATInfo.data+(cnt*block_step)) ){
            return -1;
        }
    }

    //write root dir
    if( -1 == block_write(g_superBlockInfo.fat_block_num+1, &g_rootDirInfo) ){
        return -1;
    }

    //write all data
    for(uint16_t cnt = 0; cnt < g_superBlockInfo.data_block_num; ++cnt){
        if( -1 == block_write(g_superBlockInfo.data_block_idx+cnt, 
                    g_all_data.data_all[cnt]) ){
            return -1;
        }
    }

    return block_disk_close();
}

int fs_info(void)
{
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", g_superBlockInfo.block_num_total);
    printf("fat_blk_count=%d\n", g_superBlockInfo.fat_block_num);
    printf("rdir_blk=%d\n", g_superBlockInfo.root_dir_block_idx);
    printf("data_blk=%d\n", g_superBlockInfo.data_block_idx);
    printf("data_blk_count=%d\n", g_superBlockInfo.data_block_num);
    printf("fat_free_ratio=%d/%d\n", _get_free_FAT_num(),
        g_superBlockInfo.data_block_num);
    printf("rdir_free_ratio=%d/%d\n", FS_FILE_MAX_COUNT-g_fileNumTotal, FS_FILE_MAX_COUNT);

    return 0;
}

int fs_create(const char *filename)
{
//    printf("%s\n", __FUNCTION__);
    if(FS_FILE_MAX_COUNT <= g_fileNumTotal){
        return -1;
    }

    if(0 != _check_filename(filename)){
        return -1;
    }

    if(-1 != _search_file_by_filename(filename)){
        //already existed
        return -1;
    }

    int16_t idx = _find_empty_entry();
    if(-1 == idx){
        return -1;
    }

    strncpy(g_rootDirInfo.files[idx].filename, filename, strlen(filename));
    g_rootDirInfo.files[idx].file_size = 0;
    g_rootDirInfo.files[idx].start_data_block_idx = FAT_EOC;

    g_fileNumTotal++;
    return 0;
}

int fs_delete(const char *filename)
{
//    printf("%s\n", __FUNCTION__);
    if(0 != _check_filename(filename)){
        return -1;
    }

    int16_t file_idx = _search_file_by_filename(filename);
    if(-1 == file_idx){
        //not found
        return -1;
    }

    int16_t idx = _find_openedFile_by_name(filename);
    if(-1 != idx){
        //opened
        return -1;
    }

    //clear FAT
    uint16_t tmp = 0;
    uint16_t next_idx = g_rootDirInfo.files[file_idx].start_data_block_idx;
    //delete file content>?? TODO
    while(FAT_EOC != g_FATInfo.data[next_idx]){
        tmp = next_idx;
        next_idx = g_FATInfo.data[next_idx];
        g_FATInfo.data[tmp] = FAT_EOC;
    }

    //delete
    memset(g_rootDirInfo.files[file_idx].filename, 0, FS_FILENAME_LEN);
    g_rootDirInfo.files[file_idx].file_size = 0;
    g_rootDirInfo.files[file_idx].start_data_block_idx = FAT_EOC;

    g_fileNumTotal--;
    return 0;
}

int fs_ls(void)
{
    printf("FS Ls:\n");

    for(uint16_t idx = 0; idx < FS_FILE_MAX_COUNT; ++idx){
        if(0 != g_rootDirInfo.files[idx].start_data_block_idx){
            printf("file: %s, size: %d, data_blk: %d\n",
                g_rootDirInfo.files[idx].filename,
                g_rootDirInfo.files[idx].file_size,
                g_rootDirInfo.files[idx].start_data_block_idx);
        }
    }

    return 0;
}

int fs_open(const char *filename)
{
//    printf("%s\n", __FUNCTION__);
    if(0 != _check_filename(filename)){
        return -1;
    }

    if(FS_OPEN_MAX_COUNT <= g_openedFileNum){
        return -1;
    }

    int16_t file_idx = _search_file_by_filename(filename);
    if(-1 == file_idx){
        //not found
        return -1;
    }

    int16_t open_idx = _find_space_for_open_file();
    if(-1 == open_idx){
        return -1;
    }

    File_Des* fDes = (File_Des*)malloc(sizeof(File_Des));
    if(NULL == fDes){
        return -1;
    }

    fDes->idx = file_idx;
    fDes->offset = 0;
    g_openedFiles[open_idx] = fDes;

    g_openedFileNum++;
    return open_idx;
}

int fs_close(int fd)
{
//    printf("%s\n", __FUNCTION__);
    if(FS_FILE_MAX_COUNT <= fd){
        return -1;
    }

    //get file des
    File_Des* fDes = g_openedFiles[fd];
    if(NULL == fDes){
        return -1;
    }

    free(fDes);

    g_openedFiles[fd] = NULL;
    g_openedFileNum--;
    return 0;
}

int fs_stat(int fd)
{
//    printf("%s\n", __FUNCTION__);
    if(FS_FILE_MAX_COUNT <= fd){
        return -1;
    }

    //get file des
    File_Des* fDes = g_openedFiles[fd];
    if(NULL == fDes){
        return -1;
    }

    return g_rootDirInfo.files[fDes->idx].file_size;
}

int fs_lseek(int fd, size_t offset)
{
//    printf("%s\n", __FUNCTION__);
    if(FS_FILE_MAX_COUNT <= fd){
        return -1;
    }

    //get file des
    File_Des* fDes = g_openedFiles[fd];
    if(NULL == fDes){
        return -1;
    }

    uint32_t file_size = g_rootDirInfo.files[fDes->idx].file_size;
    if( (offset+fDes->offset) > file_size ){
        return -1;
    }

    fDes->offset += offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
//    printf("%s\n", __FUNCTION__);
    if( (NULL == buf)||(0 == count) ){
        return 0;
    }

    if(FS_FILE_MAX_COUNT <= fd){
        return -1;
    }

    //get file des
    File_Des* fDes = g_openedFiles[fd];
    if(NULL == fDes){
        return -1;
    }

    File_Entry* pFE = &(g_rootDirInfo.files[fDes->idx]);
    uint32_t write_len = count;
    uint8_t tmp_buf[BLOCK_SIZE] = {0};
    uint32_t write_cnt = 0;

    uint16_t start_block_idx = _get_block_idx_for_new_pos(fDes->idx, fDes->offset, 0);
    if(FAT_EOC == start_block_idx){
        //new file
        start_block_idx = _find_empty_FAT();
        pFE->start_data_block_idx = start_block_idx;
        g_FATInfo.data[pFE->start_data_block_idx] = FAT_EOC;
    }

    uint32_t add_len = 0;
    if(pFE->file_size < fDes->offset+write_len){
        add_len = (fDes->offset+write_len)-pFE->file_size;
    }

    if(BLOCK_SIZE < (fDes->offset+write_len)){
        uint16_t block_step = (fDes->offset+write_len)/BLOCK_SIZE;
        uint32_t remain_len = (fDes->offset+write_len)%BLOCK_SIZE;

        //data in first block
        block_read(start_block_idx, tmp_buf);
        memcpy(tmp_buf+fDes->offset, buf, BLOCK_SIZE-fDes->offset);
        block_write(start_block_idx, tmp_buf);
        write_cnt += BLOCK_SIZE-fDes->offset;

        //data in middle blocks
        uint16_t old_block_idx = start_block_idx;
        uint16_t block_idx = g_FATInfo.data[old_block_idx];
        for(uint16_t cnt = 0; cnt < block_step; ++cnt){
            if(FAT_EOC == block_idx){
                block_idx = _find_empty_FAT();
                g_FATInfo.data[old_block_idx] = block_idx;
            }

            block_read(block_idx, tmp_buf);
            memcpy(tmp_buf, buf+write_cnt, BLOCK_SIZE);
            block_write(block_idx, tmp_buf);
            write_cnt += BLOCK_SIZE;
            old_block_idx = block_idx;
            block_idx = g_FATInfo.data[block_idx];
        }

        //data in last block
        block_idx = g_FATInfo.data[block_idx];
        if(FAT_EOC == block_idx){
            block_idx = _find_empty_FAT();
            g_FATInfo.data[old_block_idx] = block_idx;
        }
        block_read(block_idx, tmp_buf);
        memcpy(tmp_buf, buf+write_cnt, remain_len);
        block_write(block_idx, tmp_buf);
        write_cnt += remain_len;
    }
    else{
        //write data in this block
        block_read(start_block_idx, tmp_buf);
        memcpy(tmp_buf+fDes->offset, buf, write_len);
        block_write(start_block_idx, tmp_buf);
        write_cnt += write_len;
    }

    pFE->file_size += add_len;
    fDes->offset += write_cnt;
    return write_cnt;
}

int fs_read(int fd, void *buf, size_t count)
{
//    printf("%s\n", __FUNCTION__);
    if( (NULL == buf)||(0 == count) ){
        return 0;
    }

    if(FS_FILE_MAX_COUNT <= fd){
        return -1;
    }

    //get file des
    File_Des* fDes = g_openedFiles[fd];
    if(NULL == fDes){
        return -1;
    }

    File_Entry* pFE = &(g_rootDirInfo.files[fDes->idx]);
    uint32_t file_remain_len = pFE->file_size - fDes->offset;
    uint32_t read_len = my_min(file_remain_len, count);
    uint16_t start_block_idx = _get_block_idx_for_new_pos(fDes->idx, fDes->offset, 0);
    uint8_t tmp_buf[BLOCK_SIZE] = {0};
    uint32_t read_cnt = 0;

    if(BLOCK_SIZE < (fDes->offset+read_len)){
        uint16_t block_step = (fDes->offset+read_len)/BLOCK_SIZE;
        uint32_t remain_len = (fDes->offset+read_len)%BLOCK_SIZE;

        //data in first block
        block_read(start_block_idx, tmp_buf);
        memcpy(buf, tmp_buf+fDes->offset, BLOCK_SIZE-fDes->offset);
        read_cnt += BLOCK_SIZE-fDes->offset;

        //data in middle blocks
        uint16_t block_idx = g_FATInfo.data[start_block_idx];
        for(uint16_t cnt = 0; cnt < block_step; ++cnt){
            if(FAT_EOC != block_idx){
                block_read(block_idx, tmp_buf);
                memcpy(buf+read_cnt, tmp_buf, BLOCK_SIZE);
                read_cnt += BLOCK_SIZE;
                block_idx = g_FATInfo.data[block_idx];
            }
        }

        //data in last block
        block_idx = g_FATInfo.data[block_idx];
        memcpy(buf+read_cnt, tmp_buf, remain_len);
        read_cnt += remain_len;
    }
    else{
        //read data in this block
        block_read(start_block_idx, tmp_buf);
        memcpy(buf, tmp_buf+fDes->offset, read_len);
        read_cnt += read_len;
    }

    fDes->offset += read_cnt;
    return read_cnt;
}
