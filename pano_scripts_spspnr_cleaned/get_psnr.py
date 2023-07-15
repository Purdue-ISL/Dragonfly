from itertools import count
from re import T
import numpy as np
import os
import sys
import subprocess


def main():
    if len(sys.argv) < 2:
        print("Error: python psnr.py <dir>")
        sys.exit(1)
    video_id = sys.argv[1].split("_")[1].replace("v","")
    dir = sys.argv[1].replace("./", "").replace("/", "")
    frame_list = os.listdir(dir)
    # quality --> values.
    qualities = {}
    # chunk --> quality --> tile --> values
    chunks_pspnr = {}

    idx_to_qp = {0:17, 1:22, 2:27, 3:32, 4:37, 5:42, 6:50}
    for frame_log in frame_list:
        frame_id = int(frame_log.split(".")[0].replace("f",""))
        chunk_id = int(int(frame_id-1) / 25)
        if chunks_pspnr.get(chunk_id) is None:
            chunks_pspnr[chunk_id] = {}
        tile_id = 1
        for line in open(dir+"/"+frame_log).readlines():
            pspnr_vals = line.split(",")
            for val_idx in range(0,len(pspnr_vals)):
                qp = idx_to_qp[val_idx]
                if chunks_pspnr[chunk_id].get(qp) is None:
                    chunks_pspnr[chunk_id][qp] = {}
                if chunks_pspnr[chunk_id][qp].get(tile_id) is None:
                    chunks_pspnr[chunk_id][qp][tile_id] = []
                chunks_pspnr[chunk_id][qp][tile_id].append("Inf" if "Inf" in pspnr_vals[val_idx] else float( pspnr_vals[val_idx]))
                if "Inf" in pspnr_vals[val_idx]:
                    continue
                if qualities.get(tile_id) is None:
                    qualities[tile_id] = []
                qualities[tile_id].append(float(pspnr_vals[val_idx]))
            tile_id+=1


    # tile --> qp --> chunks.
    pspnr_values_cal = {}
    for chunk_id in sorted(chunks_pspnr):
        for qp in chunks_pspnr[chunk_id]:
            for tile_id in chunks_pspnr[chunk_id][qp]:
                sum_pspnr = 0 
                for pspnr_val in chunks_pspnr[chunk_id][qp][tile_id]:
                    sum_pspnr += max(qualities[tile_id]) if "Inf" in str(pspnr_val) else float(pspnr_val)
                avg = float("%.2f"%(sum_pspnr / len(chunks_pspnr[chunk_id][qp][tile_id])))
                if pspnr_values_cal.get(tile_id) is None:
                    pspnr_values_cal[tile_id] = {}
                if pspnr_values_cal[tile_id].get(qp) is None:
                    pspnr_values_cal[tile_id][qp] = []
                pspnr_values_cal[tile_id][qp].append(avg)
    

    with open("static_pspnr_"+str(video_id)+".txt","w") as fi:
        for tile_id in sorted(pspnr_values_cal):
            for qp in sorted(pspnr_values_cal[tile_id],reverse=True):
                fi.write(str(tile_id)+"-"+str(qp)+":")
                fi.write(str(pspnr_values_cal[tile_id][qp])+"\n")




if __name__ == "__main__":
    main()
