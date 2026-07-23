clear

#directory to store build/compiled files
buildDir=../untracked/build

#directory for avr-fat source files
fatDir=src

#directory for avr-sdcard source files
sdDir=lib/sdcard/src

#directory for avr-io source files
ioDir=lib/avrio/src

#directory for helper files
hlprDir=lib/hlpr/src

#directory for test files
testDir=test

#make build directory if it doesn't exist
mkdir -p -v $buildDir


t=0.25
# -g = debug, -Os = Optimize Size
Compile=(avr-gcc -Wall -g -Os -I "includes" -I "lib/sdcard/includes" -I "lib/avrio/includes" -I "lib/hlpr/includes" -mmcu=atmega1280 -c -o)
Link=(avr-gcc -Wall -g -mmcu=atmega1280 -o)
IHex=(avr-objcopy -j .text -j .data -O ihex)


echo -e ">> COMPILE: "${Compile[@]}" "$buildDir"/avr_fat_test.o " $testDir"/avr_fat_test.c"
"${Compile[@]}" $buildDir/avr_fat_test.o $testDir/avr_fat_test.c
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


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/fat.o "$fatDir"/fat.c"
"${Compile[@]}" $buildDir/fat.o $fatDir/fat.c
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

##echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/fat_bpb.o "$fatDir"/fat_bpb.c"
##"${Compile[@]}" $buildDir/fat_bpb.o $fatDir/fat_bpb.c
##status=$?
##sleep $t
##if [ $status -gt 0 ]
##then
##    echo -e "error compiling FAT_BPB.C"
##    echo -e "program exiting with code $status"
##    exit $status
##else
##    echo -e "Compiling FAT_BPB.C successful"
##fi

echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/fat_print.o "$fatDir"/fat_print.c"
"${Compile[@]}" $buildDir/fat_print.o $fatDir/fat_print.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling FAT_PRINT.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling FAT_PRINT.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/fat_sdcard_if.o "$fatDir"/fat_sdcard_if.c"
"${Compile[@]}" $buildDir/fat_sdcard_if.o $fatDir/fat_sdcard_if.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling FAT_SDCARD_IF.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling FAT_TO_SD.C successful"
fi


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/avr_spi.o "$ioDir"/avr_spi.c"
"${Compile[@]}" $buildDir/avr_spi.o $ioDir/avr_spi.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling AVR_SPI.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling AVR_SPI.C successful"
fi

echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/prints.o "$hlprDir"/prints.c"
"${Compile[@]}" $buildDir/prints.o $hlprDir/prints.c
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


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/avr_usart.o "$ioDir"/avr_usart.c"
"${Compile[@]}" $buildDir/avr_usart.o $ioDir/avr_usart.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling AVR_USART.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling AVR_USART.C successful"
fi

echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/sd_spi_interface.o "$sdDir"/sd_spi_interface.c"
"${Compile[@]}" $buildDir/sd_spi_interface.o $sdDir/sd_spi_interface.c
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

echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/sd_spi_base.o "$sdDir"/sd_spi_base.c"
"${Compile[@]}" $buildDir/sd_spi_base.o $sdDir/sd_spi_base.c
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


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/sd_spi_rwe.o "$sdDir"/sd_spi_rwe.c"
"${Compile[@]}" $buildDir/sd_spi_rwe.o $sdDir/sd_spi_rwe.c
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


echo -e "\n\r>> COMPILE: "${Compile[@]}" "$buildDir"/sd_spi_print.o "$sdDir"/sd_spi_print.c"
"${Compile[@]}" $buildDir/sd_spi_print.o $sdDir/sd_spi_print.c
status=$?
sleep $t
if [ $status -gt 0 ]
then
    echo -e "error compiling SD_SPI_PRINT.C"
    echo -e "program exiting with code $status"
    exit $status
else
    echo -e "Compiling SD_SPI_PRINT.C successful"
fi


##$buildDir"/fat_bpb.o " add this back in if you want to compile the BPB code.
## It is currently commented out because it is not being used in the test program.

echo -e "\n\r>> LINK: "${Link[@]}" "$buildDir"/avr_fat_test.elf "$buildDir"/avr_fat_test.o  "$buildDir"/avr_spi.o "$buildDir"/sd_spi_interface.o "$buildDir"/sd_spi_base.o "$buildDir"/sd_spi_rwe.o "$buildDir"/sd_spi_print.o "$buildDir"/avr_usart.o "$buildDir"/prints.o "$buildDir"/fat.o "$buildDir"/fat_print.o "$buildDir"/fat_sdcard_if.o"
"${Link[@]}" $buildDir/avr_fat_test.elf $buildDir/avr_fat_test.o $buildDir/avr_spi.o $buildDir/sd_spi_interface.o $buildDir/sd_spi_base.o $buildDir/sd_spi_rwe.o $buildDir/sd_spi_print.o $buildDir/avr_usart.o $buildDir/prints.o $buildDir/fat.o $buildDir/fat_print.o $buildDir/fat_sdcard_if.o
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

