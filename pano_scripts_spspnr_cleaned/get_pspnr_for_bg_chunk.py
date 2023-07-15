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
    qualities = []
    # chunk --> quality --> tile --> values
    chunks_pspnr = {}

    for frame_log in frame_list:
        frame_id = int(frame_log.split(".")[0].replace("f",""))
        chunk_id = int(int(frame_id-1) / 25)
        if chunks_pspnr.get(chunk_id) is None:
            chunks_pspnr[chunk_id] = []
        for line in open(dir+"/"+frame_log).readlines():
            pspnr_vals = line.split(",")
            pspnr_val = pspnr_vals[5]
            chunks_pspnr[chunk_id].append("Inf" if "Inf" in pspnr_val else float( pspnr_val))
            if "Inf" in pspnr_val:
                continue
            qualities.append(float(pspnr_val))

    # tile --> qp --> chunks.
    pspnr_values_cal = []
    for chunk_id in sorted(chunks_pspnr):
        sum_pspnr = 0 
        for pspnr_val in chunks_pspnr[chunk_id]:
            if str(pspnr_val) == "Inf":
                pspnr_val = max(qualities)
            sum_pspnr += pspnr_val
        avg = float("%.2f"%(sum_pspnr / len(chunks_pspnr[chunk_id])))
        pspnr_values_cal.append(avg)
    

    with open("static_pspnr_chunk_"+str(video_id)+".txt","w") as fi:
        fi.write(str(pspnr_values_cal))





if __name__ == "__main__":
    main()
