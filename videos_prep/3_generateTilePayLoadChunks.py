import numpy as np
import os
import sys
import subprocess
import shlex

def main():
    if len(sys.argv) < 8:
        print("Error: python encode.py <dir> <frame_w> <frame_h> <tiles_w> <tiles_h> <qp> <FPS>")
        sys.exit(1)
    dir = sys.argv[1].replace("./","").replace("/","")
    frameWidth = int(sys.argv[2])
    frameHeight = int(sys.argv[3])
    widthTiles = int(sys.argv[4])
    heightTiles = int(sys.argv[5])
    heightRange = int(frameHeight/heightTiles)
    widthRange = int(frameWidth/widthTiles)
    FPS = int(sys.argv[7])
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
                if j == widthTiles -1 :
                    enWidthRange = frameWidth
                w = enWidthRange-stWidthRange
                h = enHeightRange-stHeightRange

                out = dir1+"/"+str(i*12+(j+1))
                try:
                    os.mkdir(out)
                except:
                    True
                inp = dir1+"/f"+str(i*12+(j+1))
                ii = 0
                c = 1
                fis = ""
                for idx in range(1,1501):
                    if (ii) % FPS == 0 and ii != 0:
                        command = ("cat "+fis+" > "+out+"/"+str(c)+".h264")
                        os.system(command)
                        ii = 0
                        c+=1
                    
                        fis = ""
                    fis += inp+"/"+str(idx)+".h264 "
                    ii += 1
                os.system("rm -r "+inp)
            




if __name__ == "__main__":
        main()
