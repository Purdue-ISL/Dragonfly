from itertools import count
import numpy as np
import os
import sys
import subprocess


def main():
    if len(sys.argv) < 2:
        print("Error: python3 psnr.py <dir> <frame_w> <frame_h> <tiles_w> <tiles_h> <qp>")
        sys.exit(1)
    dir = sys.argv[1].replace("./", "").replace("/", "")
    frameWidth = int(sys.argv[2])
    frameHeight = int(sys.argv[3])
    widthTiles = int(sys.argv[4])
    heightTiles = int(sys.argv[5])
    heightRange = int(frameHeight/heightTiles)
    widthRange = int(frameWidth/widthTiles)
    tiles_psnr = {}
    tiles_ssim = {}
    max_val = 0
    max_val_ssim = 0
    for i in range(0,  widthTiles):  # row
        for j in range(0, heightTiles):
            files = []
            files_SSIM = []
            for qp in [int(sys.argv[6])]:
                encode_dir = dir+"/QP"+str(qp)+"/"
                org_dir = dir+"/org_raw_tiles/"
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
                original_tile = org_dir+"r_"+str(i+1)+"_c_"+str(j+1)+".yuv"
                encoded_tile = encode_dir+str(i*12+(j+1))+".yuv"
                fout = str(i*12+(j+1))+"_"+str(qp)
                command = "./vqmt "+original_tile + \
                    " "+encoded_tile+" 320 160 1500 1 "+fout+" PSNR SSIM"
                files.append(fout+"_psnr.csv")
                files_SSIM.append(fout+"_ssim.csv")
                os.system("chmod +x "+original_tile)
                os.system("chmod +x "+encoded_tile)
                process = subprocess.Popen(
                    command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
                stdout, stderr = process.communicate()

                os.system("rm "+encoded_tile)
            for psnr_file in files:
                fi = open(psnr_file)
                tile_id = int(psnr_file.split("_")[0])
                qpRes = int(psnr_file.split("_")[1])
                if (tiles_psnr.get(tile_id)) is None:
                    tiles_psnr[tile_id] = {}
                if(tiles_psnr[tile_id].get(qpRes) is None):
                    tiles_psnr[tile_id][qpRes] = {}

                count_ = 0
                for line in fi.readlines():
                    if "average" in line or "frame" in line:
                        continue
                    if "inf" not in line:
                        max_val = max(max_val, float(line.split(",")[1]))
                    chunkId = int(count_ / 25)
                    if tiles_psnr[tile_id][qpRes].get(chunkId) is None:
                        tiles_psnr[tile_id][qpRes][chunkId] = []

                    tiles_psnr[tile_id][qpRes][chunkId].append(
                        line.split(",")[1])
                    count_ += 1
                os.system("rm "+psnr_file)

            for ssim_file in files_SSIM:
                fi = open(ssim_file)
                tile_id = int(ssim_file.split("_")[0])
                qpRes = int(ssim_file.split("_")[1])
                if (tiles_ssim.get(tile_id)) is None:
                    tiles_ssim[tile_id] = {}
                if(tiles_ssim[tile_id].get(qpRes) is None):
                    tiles_ssim[tile_id][qpRes] = {}

                count_ = 0
                for line in fi.readlines():
                    if "average" in line or "frame" in line:
                        continue
                    if "inf" not in line:
                        max_val_ssim = max(
                            max_val_ssim, float(line.split(",")[1]))
                    chunkId = int(count_ / 25)
                    if tiles_ssim[tile_id][qpRes].get(chunkId) is None:
                        tiles_ssim[tile_id][qpRes][chunkId] = []

                    tiles_ssim[tile_id][qpRes][chunkId].append(
                        line.split(",")[1])
                    count_ += 1
                os.system("rm "+ssim_file)

    fwrite = open(dir+"/psnr_avgs_QP"+sys.argv[6]+".txt", "w")
    for tile_id in sorted(tiles_psnr):
        for qp in sorted(tiles_psnr[tile_id], reverse=True):
            line = str(tile_id)+"-"+str(qp)+":["
            for chunk in sorted(tiles_psnr[tile_id][qp]):
                total = 0.0
                for v in tiles_psnr[tile_id][qp][chunk]:
                    if "inf" in v:
                        total += max_val
                    else:
                        total += float(v)
                line += "%.2f" % (total*1.0/25)+","
            fwrite.write(line[:-1]+"]\n")

    fwrite = open(dir+"/ssim_avgs_QP"+sys.argv[6]+".txt", "w")
    for tile_id in sorted(tiles_ssim):
        for qp in sorted(tiles_ssim[tile_id], reverse=True):
            line = str(tile_id)+"-"+str(qp)+":["
            for chunk in sorted(tiles_ssim[tile_id][qp]):
                total = 0.0
                for v in tiles_ssim[tile_id][qp][chunk]:
                    if "inf" in v:
                        total += max_val_ssim
                    else:
                        total += float(v)
                line += "%.2f" % (total*1.0/25)+","
            fwrite.write(line[:-1]+"]\n")


if __name__ == "__main__":
    main()
