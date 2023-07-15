# Pano Static *PSPNR* and Tile Grouping
This dir contains modified Pano scripts to generate static *PSPNR* (*SPSPNR*)
## Prerequisites
* [MATLAB](https://www.mathworks.com/help/install/)
  * Make sure you check Image Processing Toolbox 
* [folly](https://github.com/facebook/folly)

## Video Preparation
First, we need to cut the video into 1 second chunks. For this: 
* Place videos under directory path `videos/1/<video-Id>.mp4`. Please make sure you **rename** the videos into three digits id. For instance "videos/1/020.mp4" where 20 is the video id
* To segment the video, please run the following command:
```
matlab -nodisplay -nosplash -nodesktop -r "run('./cutChunk.m); exit;
```
* The video chunks can be found under dir path of `videos/1/<video-Id>/<chunks>`

## Generate *SPSPNR*
Now, we can generate the *SPSPNR* for all these video chunks
### Hardcoded values
Before we generate *SPPNR* values, there are couple of code changes you may want to consider first:
1. *getAllTileValueness.m*
   * line 8  → contains an array of the video ids `Vid = [list of video ids]`
   * line 19  → loops over video chunks, it is hardcoded to 60 seconds which is the duration of our videos.
 
1. *calcTileValueness.m*
   * *lines 3-6  → contain info about the video resolution (e.g., 3840x1920) and number of tiles (e.g., 12x12)
1. *CalcTileMse.m*
   * line 12  → QPrange —> list of quantization parameters (i.e., video qualities)
   * line 17  → SPSPNR = zeros(size(tiling, 1), \<# of qualities\>)
   * lines 24-25  →  width and height of the video
   * line 98  → KeySet = { … } —> list of quantization parameters (i.e., video qualities)
   * line 99  → ValueSet = [ … ]—> indices of video qualities
   * Throughout the script, "17" and "50" quantization values correspond to highest and lowest quality qps. Please change accordingly
### Run
To generate the *SPRPNR* for all tiles and all videos please run:
```
matlab -nodisplay -nosplash -nodesktop -r  "run('./getAllTileValueness.m); exit;"
```
This will output the *SPRPNR* values per frame under the follwoing dir `"PSPNR<video_id>/f<frameId>.txt"`, where each line corresponds to the *SPRPNR* per tile per quality

## *SPSPNR* per Chunk
To get the average *SPSPNR* per tile per chunk, please execute the following command:
```
python3 get_psnr.py <out_dir of getAllTileValueness.m script>
```
This will output the *SPSPNR* per tile chunk under the following dir `"./static_pspnr_<video_id>"`, each line has a format `"T-Q:[spspnr values per chunk]"` where T is tile and Q is the quantization value (i.e., quality)
# Tile Grouping
* **Build** tileGrouping.cpp using ` g++ --std=c++14 -lfolly TileGrouping.cpp -o tile_group `
* **Run** tile_group using ` ./tile_group static_pspnr_<video_id>.txt <video_id>.txt `
