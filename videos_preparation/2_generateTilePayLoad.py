import numpy as np
import os
import sys
import subprocess
import shlex


def main():
    if len(sys.argv) < 7:
        print("Error: python3 encode.py <dir> <frame_w> <frame_h> <tiles_w> <tiles_h> <qp>")
        sys.exit(1)
    dir = sys.argv[1].replace("./", "").replace("/", "")
    frameWidth = int(sys.argv[2])
    frameHeight = int(sys.argv[3])
    widthTiles = int(sys.argv[4])
    heightTiles = int(sys.argv[5])
    heightRange = int(frameHeight/heightTiles)
    widthRange = int(frameWidth/widthTiles)

    for qp in [int(sys.argv[6])]:
        dir1 = dir +"/QP"+str(qp)
        for i in range(0, widthTiles):  # row
            for j in range(0, heightTiles):
                stHeightRange = heightRange*i
                enHeightRange = heightRange*(i+1)
                stWidthRange = widthRange*j
                enWidthRange = widthRange*(j+1)
                if i == heightTiles - 1:
                    enHeightRange = frameHeight
                if j == widthTiles - 1:
                    enWidthRange = frameWidth
                w = enWidthRange-stWidthRange
                h = enHeightRange-stHeightRange
                f = dir1+"/"+str(i*12+(j+1))+".mp4"
                try:
                    tiledir = dir1+"/f"+str(i*12+(j+1))
                    os.mkdir(tiledir)
                except OSError as error:
                    True  # print "error"
                command = (
                    "ffmpeg -i "+f+" -f image2 -vcodec copy -bsf h264_mp4toannexb "+tiledir+"/%d.h264")
                process = subprocess.Popen(
                    command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)

                stdout, stderr = process.communicate()


if __name__ == "__main__":
    main()
