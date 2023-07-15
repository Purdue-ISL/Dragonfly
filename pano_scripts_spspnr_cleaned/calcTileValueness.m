function calcTileValueness(set, vid, sec)

    nGridR = 12; % number of tiles height (row divides height of the image)
    nGridC = 12; % number of tiles width
    tileW = 3840 / nGridC;
    tileH = 1920 / nGridR;

    % this generate the tiling scheme to calculate PMSE for,
    % it is set to 12x12 tiles
    tiling = [];

    for i = 1:nGridR

        for j = 1:nGridC
            tiling = [tiling; i, i, j, j];
        end

    end

    disp("calcTileMSE.m called.")

    for frameId = [10, 15, 20]
        [viewedTiles, MSE] = calcTileMse(set, vid, sec, frameId, tiling, tileW, tileH, 'real', 'real', 1);
    end

    %%%STOP HERE%%%
    disp("calcTileMSE.m returned.")
    % num_users * num_tiles * num_qualities(42 - 22 + 1)
    %for user=1:1%1:48
    %    mkdir(['ratio/',num2str(set),'/',num2str(vid-1),'/',num2str(user)]);
    %    userMSE = MSE;%squeeze(MSE(user,:,:));%squeeze(mean(MSE(:,i,qp-22+1)));
    %    valueness = (userMSE(:,42-22+1)-userMSE(:,22-22+1)) ./ 1; % 48
    %    % The difference of MSE is there, and the code rate of qp=22 and 42 is needed
    %    for i=1:size(tiling,1)
    %        if valueness(i,1)~=0
    %            temp22 = calcTileSize(set,vid,sec,tiling(i,1),tiling(i,2),tiling(i,3),tiling(i,4),tileH,tileW, 22);
    %            temp42 = calcTileSize(set,vid,sec,tiling(i,1),tiling(i,2),tiling(i,3),tiling(i,4),tileH,tileW, 42);
    %            valueness(i,1) = valueness(i,1)/(temp22-temp42);
    %        end
    %    end
    %    %save(['tileVal/',num2str(set),'/',num2str(vid),'/',num2str(sec),'.mat'],'valueness');
    %
    %    valueness(valueness<0)=0;
    %    % reshape
    %    new = zeros(12,24);
    %    for row=1:12
    %        new(row,:) = valueness(row*24-23:row*24);
    %    end
    %    %DEBUG
    %    new = new .* 10;
    %    %write to user
    %    dlmwrite(['ratio/',num2str(set),'/',num2str(vid-1),'/',num2str(user),'/',num2str(sec*30-29),'_Value_SMSE.txt'],new,' ');

    %end

    %end
