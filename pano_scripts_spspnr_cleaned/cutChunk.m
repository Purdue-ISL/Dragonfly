for set = 1

    for vid = [1] % loop over videos
        mkdir(sprintf('videos/%d/%03d', set, vid));
        video_outdir = sprintf('videos/%d/%03d', set, vid)
        qp = 15; % generate raw chunks
        command = ['ffmpeg -r 25 -i ', sprintf('videos/%d/%03d.mp4', set, vid), ...
                       ' -an -c:v libx264 -qp ', num2str(qp), ' -g 25 -vf fps=25 -f segment ', ...
                       '-segment_list ', video_outdir, '/tmp.m3u8 -segment_time 1 ', ...
                       video_outdir, '/%03d.mp4'];
        system(command)
    end

end
