# Pano Static PSPNR (how to generate?)
This dir contains modified Pano scripts to generate static PSPNR
## Prerequisites
* [MATLAB](https://www.mathworks.com/help/install/)
  * Make sure you install Image Processing Toolbox 
* [folly](https://github.com/facebook/folly)

## Video Preparation
First, we need to cut the video into 1 second chunks. For this, 
* Place videos under directory path "videos/1/\<video-Id\>.mp4". Please make sure you **rename** the videos into three digits id. For instance "videos/1/020.mp4" where 20 is the video id.
* To segment the video, please run the following command:
```
matlab -nodisplay -nosplash -nodesktop -r "run('./cutChunk.m); exit;
```
* The video chunks can be found under dir path of "videos/1/\<video-Id\>/\<chunks\>

## Generate static PSPNR
Before we generate SPPNR values, there are couple of code changes you may want to consider first:
1. *getAllTileValueness.m*
   * line 8 contains an array of the video ids `Vid = [list of video ids]`
   * line 19 loops over video chunks, it is hardcoded to 60 seconds which is the duration of our videos.
 
1. calcTileValueness.m
