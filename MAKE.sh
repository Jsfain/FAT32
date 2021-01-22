clear

#directory to store build/compiled files
buildDir=../untracked/build

#directory for source files
sourceDir=source

#make build directory if it doesn't exist
mkdir -p -v $buildDir


t=0.25
# -g = debug, -Os = Optimize Size
Compile=(avr-gcc -Wall -g -Os -I "includes/" -mmcu=atmega1280 -c -o)
Link=(avr-gcc -Wall -g -mmcu=atmega1280 -o)
IHex=(avr-objcopy -j .text -j .data -O ihex)


echo -e ">> COMPILE: "${Compile[@]}" "$buildDir"/avr_fat_test.o " $sourceDir"/avr_fat_test.c"
"${Compile[@]}" $buildDir/avr_fat_test.o $sourceDir/avr_fat_test.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling AVR_FAT_TEST.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling AVR_FAT_TEST.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/fat.o "$sourceDir"/fat.c"
"${Compile[@]}" $buildDir/fat.o $sourceDir/fat.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling FAT.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling FAT.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/fat_bpb.o "$sourceDir"/fat_bpb.c"
"${Compile[@]}" $buildDir/fat_bpb.o $sourceDir/fat_bpb.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling FAT_BPB.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling FAT_BPB.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/fat_to_sd.o "$sourceDir"/fat_to_sd.c"
"${Compile[@]}" $buildDir/fat_to_sd.o $sourceDir/fat_to_sd.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling FAT_TO_SD.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling FAT_TO_SD.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/spi.o "$sourceDir"/spi.c"
"${Compile[@]}" $buildDir/spi.o $sourceDir/spi.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling SPI.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling SPI.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/prints.o "$sourceDir"/prints.c"
"${Compile[@]}" $buildDir/prints.o $sourceDir/prints.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling PRINTS.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling PRINTS.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/usart0.o "$sourceDir"/usart0.c"
"${Compile[@]}" $buildDir/usart0.o $sourceDir/usart0.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling USART0.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling USART0.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/sd_spi_base.o "$sourceDir"/sd_spi_base.c"
"${Compile[@]}" $buildDir/sd_spi_base.o $sourceDir/sd_spi_base.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling SD_SPI_BASE.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling SD_SPI_BASE.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/sd_spi_rwe.o "$sourceDir"/sd_spi_rwe.c"
"${Compile[@]}" $buildDir/sd_spi_rwe.o $sourceDir/sd_spi_rwe.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling SD_SPI_RWE.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling SD_SPI_RWE.C successful"
fi


echo -e "\n\r>> LINK: "${Link[@]}" "$buildDir"/avr_fat_test.elf "$buildDir"/avr_fat_test.o  "$buildDir"/spi.o "$buildDir"/sd_spi_base.o "$buildDir"/sd_spi_rwe.o "$buildDir"/usart0.o "$buildDir"/prints.o "$buildDir"/fat_bpb.o "$buildDir"/fat.o "$buildDir"/fat_to_sd.o"
"${Link[@]}" $buildDir/avr_fat_test.elf $buildDir/avr_fat_test.o $buildDir/spi.o $buildDir/sd_spi_base.o $buildDir/sd_spi_rwe.o $buildDir/usart0.o $buildDir/prints.o $buildDir/fat.o $buildDir/fat_bpb.o $buildDir/fat_to_sd.o
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error during linking"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Linking successful. Output in AVR_FAT_TEST.ELF"
fi



echo -e "\n\r>> GENERATE INTEL HEX File: "${IHex[@]}" "$buildDir"/avr_fat_test.elf "$buildDir"/avr_fat_test.hex"
"${IHex[@]}" $buildDir/avr_fat_test.elf $buildDir/avr_fat_test.hex
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error generating HEX file"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "HEX file successfully generated. Output in AVR_FAT_TEST.HEX"
fi



echo -e "\n\r>> DOWNLOAD HEX FILE TO AVR"
echo "avrdude -p atmega1280 -c dragon_jtag -U flash:w:avr_fat_test.hex:i -P usb"
avrdude -p atmega1280 -c dragon_jtag -U flash:w:$buildDir/avr_fat_test.hex:i -P usb
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error during download of HEX file to AVR"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Program successfully downloaded to AVR"
fi

