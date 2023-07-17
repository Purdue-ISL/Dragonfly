import numpy as np
import os
import sys
import subprocess


def main():
    if len(sys.argv) < 2:
        print("Error: python chunks_sizes.py <dir>")
        sys.exit(1)
    video_id = sys.argv[1].split("_")[0]
    dir = sys.argv[1].replace("./", "").replace("/", "")
    # tileId --> qp --> chunkSizes
    chunks_sizes = {}
    for qp in [17, 22, 27, 32, 37, 42, 50]:
        tiles_chunks_dir = dir+"/QP"+str(qp)
        tiles_chunks_dir_list = os.listdir(tiles_chunks_dir)
        for tiles_dir in sorted(tiles_chunks_dir_list):
            if "DS" in tiles_dir:
                continue
            tileId = int(tiles_dir)
            if chunks_sizes.get(tileId) is None:
                chunks_sizes[tileId] = {}
            if chunks_sizes[tileId].get(qp) is None:
                chunks_sizes[tileId][qp] = []
            chunks_dir = tiles_chunks_dir+"/"+tiles_dir
            data = subprocess.check_output("ls -l "+chunks_dir, shell=True)
            temp_ = {}
            for line in data.split("\n"):
                if "total" in line or line == "":
                    continue
                info = line.split()
                chunkId = int(info[8].split(".")[0])
                tileSize = int(info[4])
                temp_[chunkId] = tileSize
            for chunkId in sorted(temp_):
                chunks_sizes[tileId][qp].append(temp_[chunkId])

    fwrite = open(dir+"/sizes.txt", "w")
    for tileId in sorted(chunks_sizes):
        for qp in sorted(chunks_sizes[tileId], reverse=True):
            line = str(tileId)+"-"+str(qp)+":"+str(chunks_sizes[tileId][qp])
            fwrite.write(line+"\n")


if __name__ == "__main__":
    main()
