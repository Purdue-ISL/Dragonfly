import numpy as np
import os
import sys


def main():
    if len(sys.argv) < 2:
        print("Usage error: python3 run.py <videos dir>")
        sys.exit(1)
    videos_dir = sys.argv[1].replace("/","")
    videos = os.listdir(videos_dir)
    frameWidth = 3840
    frameHeight = 1920
    widthTiles = 12
    heightTiles = 12
    qps = [17, 22, 27, 32, 37, 42, 50]
    FPS = 25
    for video in videos:
        video_path = videos_dir+"/"+video
        video_p = video.split(".")[0]+"_data"
        command = "ffmpeg -i " + \
                str(video_path)+" -filter:v crop=%d:%d:0:0,fps=%d out.yuv " %(frameWidth,frameHeight,FPS)
        os.system(command)
        os.system("python3 0_split_ffmpeg.py out.yuv "+video_p)
        os.system("rm out.yuv")
        for qp in qps:
            os.system("python3 1_encode.py %s %d %d %d %d %d" %  
                        (video_p,frameWidth,frameHeight,widthTiles,heightTiles,qp))
            os.system("python3 2_generateTilePayLoad.py  %s %d %d %d %d %d" % 
                        (video_p,frameWidth,frameHeight,widthTiles,heightTiles,qp))
            os.system("python3 3_generateTilePayLoadChunks.py %s %d %d %d %d %d %d" %  
                        (video_p,frameWidth,frameHeight,widthTiles,heightTiles,qp, FPS))
            os.system("python3 4_decode.py %s %d %d %d %d %d" % 
                        (video_p,frameWidth,frameHeight,widthTiles,heightTiles,qp))
            os.system("python3 5_psnr.py %s %d %d %d %d %d" %
                        (video_p,frameWidth,frameHeight,widthTiles,heightTiles,qp))


if __name__ == "__main__":
    main()
