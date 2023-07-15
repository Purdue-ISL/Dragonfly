function [result,sPSPNR]=CalPMSEPerTileGra(img_raw,img_target,R)%,T) 
    %This is for a specific image, not the whole image, so the image and the R matrix must be given to some parts.
    D_JND=double(abs(double(img_target)-double(img_raw)));
    C=D_JND-R;
    temp=(C>0).*(C.^2);

    temp_PSPNR=sum(temp(:)); 
    mean_PSPNR = mean(temp(:))
    sPSPNR = 20*log10(255/sqrt(mean_PSPNR));
    result=temp_PSPNR;
end