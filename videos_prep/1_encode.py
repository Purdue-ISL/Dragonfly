import numpy as np
import os
import sys
import subprocess


def main():
    if len(sys.argv) < 7:
        print("Error: python encode.py <dir> <frame_w> <frame_h> <tiles_w> <tiles_h> <qp>")
        sys.exit(1)
    dir = sys.argv[1].replace("./", "").replace("/", "")
    dir2 = dir+"/org_raw_tiles"
    frameWidth = int(sys.argv[2])
    frameHeight = int(sys.argv[3])
    widthTiles = int(sys.argv[4])
    heightTiles = int(sys.argv[5])
    heightRange = int(frameHeight/heightTiles)
    widthRange = int(frameWidth/widthTiles)

    for qp in [int(sys.argv[6])]:
        encode_dir = dir+"/QP"+str(qp)+"/"
        try:
            os.mkdir(dir+"/QP"+str(qp))
        except OSError as error:
            True
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
                o = encode_dir+str(i*12+(j+1))+".mp4"
                f = dir2+"/r_"+str(i+1)+"_c_"+str(j+1)+".yuv"

                # no B frames
                command = "ffmpeg -f rawvideo -pix_fmt yuv420p -s:v " + \
                    str(w)+"x"+str(h)+" -r 25 -i "+f + \
                    " -x264opts 'keyint=25:min-keyint=25:no-scenecut' -qp "+str(qp)+" -bf 0 -c:v libx264 "+o
                process = subprocess.Popen(
                    command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
                stdout, stderr = process.communicate()


if __name__ == "__main__":
    main()
