function [viewedTiles, MSE] = calcTileMse(set, vid, sec, frame, tiling, tileW, tileH, transRealOrPred, calcMSERealOrPred, only42and22)
    matlab.video.read.UseHardwareAcceleration('off')
    MSE = zeros(size(tiling, 1), 50 - 17 + 1); % nUser * nubmer of tiles * (42 - 22 + 1)
    viewedTiles = zeros(size(tiling, 1)); % nUser * nubmer of tiles

    % transRealOrPred indicates what viewpoint
    % is used for transmission, the only function is
    % to determine which blocks to transfer,
    % calcMSERealOrPred indicates what to use for MSE calculation

    % If only42and22 is 0, only QP=22 and 42 will be calculated.
    QPrange = [17, 22, 27, 32, 37, 42, 50];
    % input: original video, a sequence of frames, user's viewpoint at each frame

    frameAbs = (sec - 1) * 25 + frame;

    SPSPNR = zeros(size(tiling, 1), 7);

    orgChunkPath = sprintf('videos/%d/%03d/%03d.mp4', set, vid, sec - 1);

    vr = VideoReader(orgChunkPath);
    formats = VideoReader.getFileFormats();

    H = 1920; %vr.Height;
    W = 3840; %vr.Width;

    % Get the frame image from the original video
    % Also, we need to get the images from
    % the nine previous frames for the luminance calculation.

    orgFrameImgFolderPath = sprintf('image/%d/%d', set, vid);
    mkdir(orgFrameImgFolderPath);

    %lastSecVr = -1;
    for f = frameAbs - 9:frameAbs
        %the frame may be at the last second.
        if f <= (sec - 1) * 25
            command = ['ffmpeg -i ', sprintf('videos/%d/%03d/%03d.mp4', set, vid, sec - 2), ' -vf "select=eq(n\,',num2str(f - (sec-2)*30-1),')" -vframes 1 ', sprintf('%s/%04d_org.png', orgFrameImgFolderPath, f)];
            system(command);
        else
            command = ['ffmpeg -i ', sprintf('videos/%d/%03d/%03d.mp4', set, vid, sec - 1), ' -vf "select=eq(n\,',num2str(f - (sec-1)*25-1),')" -vframes 1 ', sprintf('%s/%04d_org.png', orgFrameImgFolderPath, f)];
            disp(command);
            system(command);
        end

    end

    orgFrameImg = imread(sprintf('%s/%04d_org.png', orgFrameImgFolderPath, frameAbs));
    orgFrameImg = rgb2gray(orgFrameImg);

    % this will generate the image (same frame) with different qualities qp (22 and 42),
    % and store them in the dictionary below
    qpFrameImgs = {};

    for qp = QPrange

        try
            test = qpFrameImgs{qp - 17 + 1};
        catch
            qpChunkPath = sprintf('videos/%d/%03d/%03d_%02d.mp4', set, vid, sec, qp);
            qpFrameImgPath = sprintf('image/%d/%03d/%03d/%02d_%02d.png', set, vid, sec, frame, qp);

            if ~exist(qpFrameImgPath, 'file')
                mkdir(sprintf('image/%d/%03d/%03d', set, vid, sec));

                if ~exist(qpChunkPath, 'file')
                    command = sprintf('ffmpeg -r 25 -i %s -r 25 -c:v libx264 -qp %d %s', orgChunkPath, qp, qpChunkPath);
                    system(command);
                end

                command = ['ffmpeg -i ', qpChunkPath, ' -vf "select=eq(n\,',num2str(frame-1),')" -vframes 1 ', qpFrameImgPath];
                system(command);
            end

            qpFrameImg = imread(qpFrameImgPath);
            qpFrameImgs{qp - 17 + 1} = rgb2gray(qpFrameImg);
        end

    end

    disp('chunks & images prepared.');

    % calculate the JND of static image if it doesn't exist

    if exist(sprintf('SJND/%03d/%03d/%04d.mat', set, vid, frameAbs), 'file')
        SJND = cell2mat(struct2cell(load(sprintf('SJND/%d/%03d/%04d.mat', set, vid, frameAbs))));

    else
        SJND = CalSJND_FAST_GPU2(double(orgFrameImg));
        mkdir(sprintf('SJND/%03d/%03d', set, vid));
        save(sprintf('SJND/%03d/%03d/%04d.mat', set, vid, frameAbs), 'SJND');

    end

    %writematrix(SJND,"SJND.dat","Delimiter",",");
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%STOP HERE%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    keySet = {'17', '22', '27', '32', '37', '42', '50'};
    valueSet = [1, 2, 3, 4, 5, 6, 7];
    M = containers.Map(keySet, valueSet)

    %%
    % TODO
    % SJND 3-28
    % depth 1-4
    % Fresult 1-10
    % SpeedJND
    % LuminaceJND 1
    R = SJND;
    %DEBUG
    R = R * 0.5;
    %%
    % input: video with different qp, source video, JND matrix (R)
    % output: PMSE of each tile which is viewed
    for i = 1:size(tiling, 1)
        sr = tiling(i, 1);
        er = tiling(i, 2);
        sc = tiling(i, 3);
        ec = tiling(i, 4);
        srP = tileW * (sr - 1) + 1; % ffmpeg position start from 1
        erP = tileW * er;
        scP = tileH * (sc - 1) + 1;
        ecP = tileH * ec;
        disp([num2str(srP), ',', num2str(erP), ',', num2str(scP), ',', num2str(ecP)]);
        vtcTile = zeros(1, H);
        vtcTile(scP:ecP) = 1;
        vtcIntscTrans = vtcTile == 1;
        vtcIntscCalcMSE = vtcTile == 1;
        hrzTile = zeros(1, W);
        hrzTile(srP:erP) = 1;
        hrzIntscTrans = hrzTile == 1;
        hrzIntscCalcMSE = hrzTile == 1;

        if sum(vtcIntscTrans(:)) * sum(hrzIntscTrans(:)) > 0
            viewedTiles(i) = 1;
        end

        if sum(vtcIntscCalcMSE(:)) * sum(hrzIntscCalcMSE(:)) == 0
            MSE(i, :) = zeros(1, 50 - 17 + 1);
            SPSPNR(i, :) = zeros(1, 7);
        else
            oneTileMSE = zeros(1, 50 - 17 + 1);
            sPSPNR = zeros(1, 7);

            if sum(vtcIntscTrans(:)) * sum(hrzIntscTrans(:)) > 0

                for qp = QPrange
                    [oneTileMSE(qp - 17 + 1), sPSPNR(M(num2str(qp)))] = CalPMSEPerTileGra(orgFrameImg(vtcIntscCalcMSE, hrzIntscCalcMSE), qpFrameImgs{qp - 17 + 1}(vtcIntscCalcMSE, hrzIntscCalcMSE), R(vtcIntscCalcMSE, hrzIntscCalcMSE)); %,TcalcMSE(vtcIntscCalcMSE,hrzIntscCalcMSE));
                end

            else
                [oneTileMSE(qp - 17 + 1), sPSPNR(M(num2str(qp)))] = CalPMSEPerTileGra(orgFrameImg(vtcIntscCalcMSE, hrzIntscCalcMSE), qpFrameImgs{50 - 17 + 1}(vtcIntscCalcMSE, hrzIntscCalcMSE), R(vtcIntscCalcMSE, hrzIntscCalcMSE)); %,TcalcMSE(vtcIntscCalcMSE,hrzIntscCalcMSE));
            end

            MSE(i, :) = oneTileMSE;
            SPSPNR(i, :) = sPSPNR;
        end

    end

    disp(['MSE set ', num2str(set), ' vid ', num2str(vid), ' sec ', num2str(sec), ' frame ', num2str(frame)]);
    %writematrix(SJND,'SJND.dat','Delimiter',',');
    fname = sprintf('PSPNR_v%03d/f%d.txt', vid, frameAbs);
    writematrix(SPSPNR, fname, 'Delimiter', ',');
    disp("Done");

    command = ['rm ', sprintf('%s/*', orgFrameImgFolderPath)];
    system(command);

end
