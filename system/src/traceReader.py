
import time

fi = open("/home/ehab/Desktop/log_test.txt","r")
prev_time = 0
fw = open("/home/ehab/Desktop/Project-V360/system/log_w.txt","a")
for line in fi.readlines():
    data = line.split(",")
    if prev_time == 0:
        prev_time = int(data[0])
        time_diff = 0
    else:
        time_diff = int(data[0]) - prev_time
        prev_time = int(data[0])
    if time_diff == 0:
        continue
    time.sleep(40*1.0/1e3)
    fw.write(str(int(data[0]))+","+str(int(float(data[1])))+","+str(int(float(data[2])))+"\n")
    #fw.close()