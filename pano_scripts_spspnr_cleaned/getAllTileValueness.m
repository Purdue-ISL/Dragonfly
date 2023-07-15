clear all;
close all;
clc;

warning('off', 'all');

Set = 1;
Vid = [1];

% PANO has two sets of videos
% Set 1 (2) contains 9 (8) videos
% This will radomly pick 10 seconds from each video to calcuate efficiency score for.
for set = Set

    for vid = Vid
        command = sprintf('mkdir PSPNR_v%03d', vid);
        system(command);

        for sec = 0:60

            try
                secString = sprintf('%03d', sec - 1);
                vidString = sprintf('%03d', vid);
                vr = VideoReader(['./videos/', num2str(set), '/', num2str(vidString), '/', secString, '.mp4']);
            catch
                continue;
            end

            disp("calcTileValueness : Call");
            calcTileValueness(set, vid, sec);
            disp("calcTileValueness : Done");

        end

    end

end
