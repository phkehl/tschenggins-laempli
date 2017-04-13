{
    const uint32_t sector = 128; // within 1st MB
    //const uint32_t sector = 260; // beyond 1st MB
    const uint32_t address = SPI_FLASH_SEC_SIZE * sector;
    SpiFlashOpResult res;

    char testdata[] = "hallo welt!";
    char readdata[20];

    tic(0);
    res = spi_flash_erase_sector(sector);
    DEBUG("erase sect %u at 0x%08x: res=%d %ums", sector, address, res, toc(0));

    tic(0);
    res = spi_flash_write(address, (void *)testdata, sizeof(testdata));
    DEBUG("write sect %u at 0x%08x: res=%d %ums", sector, address, res, toc(0));

    tic(0);
    testdata[0] = 'X';
    res = spi_flash_write(address, (void *)testdata, sizeof(testdata));
    DEBUG("write sect %u at 0x%08x: res=%d %ums", sector, address, res, toc(0));

    tic(0);
    res = spi_flash_read(address, (void *)readdata, sizeof(testdata));
    DEBUG("read sect %u at 0x%08x: res=%d %ums --> %s", sector, address, res, toc(0), readdata);

    const char *hmmm = (void *)address;
    DEBUG("hmmm %p", hmmm);



}
