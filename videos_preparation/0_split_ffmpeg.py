import numpy as np
import os
import sys

def main():
    if len(sys.argv) < 3:
        print("Usage error: python split_ffmpeg.py <yuv_file> <video_dir>")
        sys.exit(1)
    video = open(sys.argv[1], 'rb')
    frameWidth = 3840
    frameHeight = 1920
    widthTiles = 12
    heightTiles = 12

    tile_width = frameWidth / widthTiles
    tile_height = frameHeight / heightTiles
    out_dir = str(sys.argv[2])
    try:
        os.mkdir(out_dir)
    except OSError as error:
        True
    dir = out_dir+"/org_raw_tiles"
    try:
        os.mkdir(dir)
    except OSError as error:
        True

    for w in range(0, widthTiles):
        for h in range(0, heightTiles):
            crop_w = w * tile_width
            crop_h = h * tile_height

            output_name = dir + "/r_"+str(h+1)+"_c_"+str(w+1)+".yuv"
            cmd = 'ffmpeg -f rawvideo -pix_fmt yuv420p -s:v ' + str(frameWidth) + 'x' + str(frameHeight) + \
                ' -i out.yuv -filter:v "crop=' + \
                str(tile_width)+':'+str(tile_height) + ':' + \
                str(crop_w) + ':' + str(crop_h) + '" ' + output_name
            print(cmd)
            os.system(cmd)


if __name__ == "__main__":
    main()
