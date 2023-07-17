# Videos Preparation (Tiling & PSNR/SSIM)
This dir contains python script to segment videos temporally into chunks and spatially into tiles,
or as we refer to in out paper tile-chunks

# Prerequisites
* [Python3](https://www.python.org/downloads/)
* [FFMPEG](https://ffmpeg.org)
* [**V**ideo **Q**uality **M**easurement **T**ool (VQMT) ](https://github.com/rolinh/VQMT)

## Scripts
First, lets walk through each script, what its purpose, and what changes you might want to consider before executing:
* 0_split_ffmpeg.py: This scripts spatially divide the raw video into raw tiles
* 1_encode.py: encodes raw tiles into different quality using QP ffmpeg parameter
* 2_generateTilePayLoad.py: extract frame payload from encoded tiles
* 3_generateTilePayLoadChunks: group frames into chunks of specific length (e.g. 25 frames = 1sec)
* 4_decode.py: transform the encoded tile video back to raw
* 5_psnr.py: generate PSNR/SSIM for each tile-video
  * line 43-44: tile_width(height) set to 320(160), and number of frames to 1500 (i.e., 60sec * 25frames/1sec).
* **run.py**: takes as input video_dir, and execute all of the aforementioned scripts in sequence
  * lines 12-17: video resolution (e.g., 3840 x 1920), number of tiles across X and Y axes (e.g., 12x12),
    frame per second (FPS), and quantization param values (e.g., qps = [17, 22, 27, 32, 37, 42, 50]).
## How to run?
1. Please place all your videos in a single directory
1. Rename videos with corresponding ids instead (**Recommended**)
1. run the command `python3 run.py <video_dir>`
### output
This will generate <video_id>_data directory that contains:
```
<video_id>_data/
               ├── org_raw_tiles: contains the tiles raw videos (feel free to detelte to save on storage)
               ├── psnr_avg_QP<qp>.txt: contains the avg psnr per tile per chunk.
               ├── ssim_avg_QP<qp>.txt: contains the avg ssim per tile per chunk.
               ├── QP<qp>: directory per quality
                   ├── <tile_id>: directory per tile
                       ├── <chunk_id>.h264: tile-chunk
```
