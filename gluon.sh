# Gluon Kernel Compiler
red='tput setaf 1'
green='tput setaf 2'
yellow='tput setaf 3'
blue='tput setaf 4'
violet='tput setaf 5'
cyan='tput setaf 6'
white='tput setaf 7'
normal='tput sgr0'
bold='setterm -bold'
date="date"
KERNEL_BUILD="Gluon_Kernel_delo3geur-`date '+%Y-%m-%d---%H-%M'`" 
echo $1 > VERSION	
VERSION='cat VERSION'
$yellow
MODULES=./gluon_works/system/lib/modules
TOOLCHAIN=../../toolchain/linaro/bin/arm-eabi
$blue
echo " |========================================================================| "
echo " |*************************** GLUON KERNEL *******************************| "
echo " |========================================================================= "
$cyan
echo " |========================================================================| "
echo " |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Gluon Works ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| "
echo " |========================================================================| "
$red
echo " |========================================================================| "
echo " |~~~~~~~~~~~~~~~~~~~~~~~~~~~~ DEVELOPER ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| "
$cyan
echo " |%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Yajnab %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%| "
$red
echo " |=========================== XDA-DEVELOPERS =============================| "
echo " |========================= Github.com/Yajnab ============================| "
echo " |========================================================================| "
$yellow
$bold
echo " |========================================================================| "
echo " |======================== COMPILING GLUON KERNEL ========================| "
echo " |========================================================================| "
$normal


if [ -n VERSION ]; then
echo "Release version is 0"
echo "0" > .version
else 
echo "Release version is $VERSION" 
echo $VERSION > .version
rm VERSION
fi

$cyan
echo "Cleaning"
$violet
make clean
make mrproper
cd out
rm kernel.zip
cd ../
cd output
rm boot.img
cd ../
clear
$cyan
echo " Making config"
$violet
ARCH=arm CROSS_COMPILE=../../toolchain/linaro/bin/arm-eabi- make delos_gluon_defconfig
clear


$cyan
echo "Making the zImage-the real deal"
$violet
ARCH=arm CROSS_COMPILE=../../toolchain/linaro/bin/arm-eabi- make -j64 CONFIG_DEBUG_SECTION_MISMATCH=y
clear
$cyan 
mkdir gluon_works/bootimage/source_img
echo "Processing the Bootimage"
cp input_bootimage/boot.img gluon_works/bootimage/source_img/boot.img
echo "Extraction of the Boot.img"
$violet

cd gluon_works/bootimage
rm -rf unpack
rm -rf output
rm -rf boot
clear
mkdir unpack
mkdir output
mkdir boot
tools/unpackbootimg -i source_img/boot.img -o unpack
cd boot
cd ../../../
$cyan
echo "Copying output files"
$violet
mv arch/arm/boot/zImage boot.img-zImage
rm gluon_works/bootimage/unpack/boot.img-zImage	
cp boot.img-zImage gluon_works/bootimage/unpack	
rm boot.img-zImage

cd gluon_works
mkdir system
cd system
mkdir lib
cd lib
mkdir modules
cd ../../../
find -name '*.ko' -exec cp -av {} $MODULES/ \;

clear

$cyan
echo "Stripping Modules"
$violet
cd $MODULES
for m in $(find . | grep .ko | grep './')
do echo $m
$TOOLCHAIN-strip --strip-unneeded $m
done
clear
cd ../../../

clear
$red
echo " Making boot.img"
cd bootimage
$violet
rm -rf ../../output/boot.img
tools/mkbootimg --kernel unpack/boot.img-zImage --cmdline 'androidboot.hardware=qcom loglevel=1 vmalloc=200M' --pagesize 4096 --base 0x00200000 --ramdisk_offset 0x01300000 --ramdisk unpack/boot.img-ramdisk-new.gz -o ../../output/boot.img
rm -rf unpack
rm -rf output
rm -rf boot
cd ../../
$white
echo "Making Flashable Zip"
rm -rf out
mkdir out
mkdir out/system
mkdir out/system/lib
cp -avr gluon_works/flash/META-INF/ out/
cp -avr gluon_works/system/lib/modules/ out/system/lib/
cp output/boot.img out/boot.img
cd out
zip -r $KERNEL_BUILD.zip *
rm -rf META-INF
rm -rf system
rm boot.img
cd ../



$blue
echo "Cleaning"
$violet
make clean mrproper
rm -rf gluon_works/bootimage/unpack
rm -rf gluon_works/bootimage/output
rm -rf gluon_works/bootimage/boot
rm -rf gluon_works/bootimage/source_img
rm VERSION
clear
echo " |============================ F.I.N.I.S.H ! =============================|"
$red
echo " |==========================Flash it and Enjoy============================| "
$blue
echo " |==========Don't seek readymade goodies, try to make something new=======| "
$cyan
echo " |==============================Gluon Works===============================| "
$red
echo " |================================Credits=================================| "
echo " |~~~~~~~~~~~~~~~~~~~~~~Dr.Nachiketa Bandyopadhyay(My Father)~~~~~~~~~~~~~| "
echo " |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~My Computer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| "
echo " |~~~~~~~~~~~~~~~~~~~~~~~~~Samsung Galaxy Fit(Beni)~~~~~~~~~~~~~~~~~~~~~~~| "
echo " |========================================================================| "
$violet
echo " |========================================================================| "
echo " |********************Vishwanath Patil(He taught me all)******************| "
echo " |****************************Aditya Patange(Adipat)**********************| "
echo " |************************Sarthak Acharya(sakindia123)********************| "
echo " |****************************Teguh Soribin(tjstyle)**********************| "
echo " |****************************Yanuar Harry(squadzone)*********************| "
echo " |*********************************faux123********************************| "
echo " |****************************Linux Torvalds(torvalds)********************| "
echo " |========================================================================| "

$normal


