function [result]=CalSJND_FAST_GPU2(img)
B=[1,1,1,1,1;       % the operator for calc. the average background luminance.
   1,2,2,2,1;
   1,2,0,2,1;
   1,2,2,2,1;
   1,1,1,1,1];

G1=[0,0,0,0,0;      %luminance difference in vertical direction
    1,3,8,3,1;
    0,0,0,0,0;
    -1,-3,-8,-1,-1;
    0,0,0,0,0];

G2=[0,0,1,0,0;      %Main diagonal luminance difference
    0,8,3,0,0;
    1,3,0,-3,-1;
    0,0,-3,-8,0;
    0,0,-1,0,0];

G3=[0,0,1,0,0;      %Sub-diagonal brightness difference
    0,0,3,8,0;
    -1,-3,0,3,1;
    0,-8,-3,0,0;
    0,0,-1,0,0];

G4=[0,1,0,-1,0;     %Brightness difference in the horizontal direction
    0,3,0,-3,0;
    0,8,0,-8,0;
    0,3,0,-3,0;
    0,1,0,-1,0];

lambda=0.5;
T0=17;
gama=3/128;

%Enter GPU
GPU_lambda=(lambda);
GPU_T0=(T0);
GPU_gama=(gama);
GPU_G1=(G1);
GPU_G2=(G2);
GPU_G3=(G3);
GPU_G4=(G4);
GPU_img=(img);
GPU_B=(B);
GPU_BG=imfilter(GPU_img, GPU_B, 'corr')./32;
GPU_MG1=abs(imfilter(GPU_img, GPU_G1, 'corr'))./16;
GPU_MG2=abs(imfilter(GPU_img, GPU_G2, 'corr'))./16;
GPU_MG3=abs(imfilter(GPU_img, GPU_G3, 'corr'))./16;
GPU_MG4=abs(imfilter(GPU_img, GPU_G4, 'corr'))./16;
GPU_MG= GPU_MG1.*((GPU_MG1>=GPU_MG2).*(GPU_MG1>=GPU_MG3).*(GPU_MG1>=GPU_MG4))+GPU_MG2.*((GPU_MG2>GPU_MG1).*(GPU_MG2>=GPU_MG3).*(GPU_MG2>=GPU_MG4))+GPU_MG3.*((GPU_MG3>GPU_MG2).*(GPU_MG3>GPU_MG1).*(GPU_MG3>=GPU_MG4))+GPU_MG4.*((GPU_MG4>GPU_MG2).*(GPU_MG4>GPU_MG3).*(GPU_MG4>GPU_MG1));

%Calculate f1
GPU_alpha=GPU_BG.*0.0001+0.115;
GPU_beta=GPU_lambda-GPU_BG.*0.01;
GPU_f1=GPU_MG.*GPU_alpha+GPU_beta;
GPU_f2=(GPU_T0.*(1-(GPU_BG./127).^(0.5))+3).*(GPU_BG<=127)+(GPU_gama.*(GPU_BG-127)+3).*(GPU_BG>127);

GPU_R=max(GPU_f1,GPU_f2);
result=gather(GPU_R);
disp(size(result));
disp("done");
end