int openDisk(u_int disk, u_int a, u_int b, u_int c);
int readBlock(u_int offset, u_int len, void *block);
int writeBlock(u_int offset, u_int len, void *block);
void closeDisk(void);

struct buf;
int bread(u_int dev, u_int blk, u_int len, struct buf **bp);
void bdwrite(struct buf *bp);
void brelse(struct buf *bp);
