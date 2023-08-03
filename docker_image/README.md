# Dragonfly Image &ensp;[![doi](https://badgen.net/badge/DOI/10.1145%2F3603269.3604876/green)](https://doi.org/10.1145/3603269.3604876)
 


Docker image to build and evaluate Dragonfly, a new 360º video streaming system.

This accompanies the paper *"Dragonfly: Higher Perceptual Quality For Continuous 360° Video Playback"*. Ehab Ghabashneh, Chandan Bothra, Ramesh Govindan, Antonio Ortega, and Sanjay Rao. In Proceedings of the ACM Special Interest Group on Data Communication, SIGCOMM ’23, New York, NY, USA. If you use this artifact, please cite: 

```
@inproceedings{Ghabashneh_Dragonfly_2023,
  author    = {Ghabashneh, Ehab and Bothra, Chandan and Govindan, Ramesh and Ortega, Antonio and Rao, Sanjay},
  title     = {Dragonfly: Higher Perceptual Quality For Continuous 360° Video Playback},
  year      = {2023},
  url       = {https://doi.org/10.1145/3603269.3604876},
  doi       = {10.1145/3603269.3604876},
  booktitle = {Proceedings of the ACM Special Interest Group on Data Communication},
  series    = {SIGCOMM '23}
}

```

## Image Info

This image includes:

- Dragonfly, Pano, Flare, Two Tier source codes
- 7 videos (tiled and pre-encoded into multiple qualities) [[1]](https://dl.acm.org/doi/abs/10.1145/3304109.3325812)
- 30 User trajectories [[1]](https://dl.acm.org/doi/abs/10.1145/3304109.3325812)
- 21 Bandwidth traces [[2]](https://ieeexplore.ieee.org/document/7546928)[[3]](https://dl.acm.org/doi/abs/10.1145/3339825.3394938)
- Scripts to regenerate results from our SIGCOMM '23 paper

## Prerequisite

- Linux OS based machine
- Install Docker engine
  - For MACOSX users: run `brew cask install docker`
  - For Ubuntu users: run `sudo apt install docker.io`
 

## Download and Run Docker Image

- To **Download** the latest Dragonfly image, run the command below or you can refer to [this dockerhub repo](https://hub.docker.com/r/eghabash/dfly/tags) to track/download old versions of the image
  ```
  sudo docker pull eghabash/dfly
  ```
- To **Run** the image, run the following command
  ```
  sudo docker run --cap-add=NET_ADMIN --privileged  --device /dev/net/tun:/dev/net/tun -i -t eghabash/dfly
  ```
  Couple of notes,

* If you are using remote linux machine, we recommend running the image inside [screen](https://linux.die.net/man/1/screen) shell
* In case of unresposive shell, you can run `sudo docker exec -it <container-id> bash` to gain access back to docker's shell. To retrieve `<container-id>`, please run `sudo docker ps` command

## Build and Run Dragonfly

### Build dependencies and source code 
Once inside the image shell, please run `cd home/dfly && ./bash.sh` to (i) download and build all dependecies, and (ii) build Dragonfly source code. Please note that **dfly password** is `1` (not well-protected ha!)

### Evaluate Dragonfly

To reproduce evaluation results from [our paper](https://doi.org/10.1145/3603269.3604876), it takes more than a week of conducting experiements (175 hours to be precise). Therefore, we suggest first to evaluate with smaller sets of traces and users for which we provide the scripts

**Caveat**: While the estimated experiment time for the smaller dataset is much less of approximately 6 hours (i.e., <2 hours per major experiment below), the outcome of this evaluation may not be an identical to these from the full dataset. Yet, we expect the patterns of both datasets to coincide

- To reproduce schemes comparison evaluation results [§4.3 Figure 9], please run:<br/>
  **Full dataset**: &ensp;&nbsp; `python3 run_eval_main_results.py` <br/>
  **Small dataset**: `python3 run_eval_partial_main_results.py`<br/>
  _Output directories:_ `/home/dfly/main-results` and `/home/dfly/main-partial-results`
- To reproduce sPSPNR evaluation results [§4.3 Figure 10], please run:<br/>
  **Full dataset**: &ensp;&nbsp; `python3 run_eval_spspnr_results.py` <br/>
  **Small dataset**: `python3 run_eval_partial_spspnr_results.py`<br/>
  _Output directories:_ `/home/dfly/spspnr-results` and `/home/dfly/spspnr-partial-results`

- To reproduce Irish dataset evaluation results [§4.3 Figure 11], please run:<br/>
  **Full dataset**: &ensp;&nbsp; `python3 run_eval_irish_results.py` <br/>
  **Small dataset**: `python3 run_eval_partial_irish_results.py`<br/>
  _Output directories:_ `/home/dfly/irish-results ` and `/home/dfly/irish-partial-results `
- To reproduce Ablation study evaluation results [§4.4 Figure 12-13], please run:<br/>
  **Full dataset**: &ensp;&nbsp; `python3 run_eval_ablation_results.py` <br/>
  **Small dataset**: `python3 run_eval_partial_ablation_results.py`<br/>
  _Output directories:_ `/home/dfly/ablation-results ` and `/home/dfly/ablation-partial-results `

**Note**: please feel free to terminate the evaluation script at any point. And, be assured upon re-run it will resume from the point of interruption

### Plotting results

We included python scripts to regenerate the figures from our evaluation section. Please **`cd`** to one of the following directories to find the corressponding plotting scripts:<br/>
&ensp;&ensp;&ensp;**Full dataset**: &ensp;&nbsp; `cd /home/dfly/plot_figures_result_full` <br/>
&ensp;&ensp;&ensp;**Small dataset**: `cd /home/dfly/plot_figures_result_partial`<br/>

- To plot the figures, please run:<br/>
  **Small dataset**

  ```
  python3 Figure_9_a.py ../main-partial-results/ &&\
  python3 Figure_9_b.py ../main-partial-results/ &&\
  python3 Figure_9_c.py   ../main-partial-results/ &&\
  python3 Figure_10.py ../spspnr-partial-results/ &&\
  python3 Figure_11.py ../irish-partial-results/ &&\
  python3 Figure_12_a.py ../ablation-partial-results/ &&\
  python3 Figure_12_b.py ../ablation-partial-results/ &&\
  python3 Figure_12_c.py   ../ablation-partial-results/ &&\
  python3 Figure_13_a.py   ../ablation-partial-results/ &&\
  python3 Figure_13_b.py   ../ablation-partial-results/
  ```

  **Output:** This will plot all figures following the paper numberings, and save them under the following directory:

  ```
  /home/dfly/plot_figures_result_partial/figs_partial/Figure_<number>_partial.png
  ```

  Likewise for the **full dataset**, except that you should replace the `partial` word with `full` for the above commands and path

- To **copy figures from docker**, please run:
  ```
  docker cp <container_id>:/home/dfly/plot_figures_result_partial/figs_partial/ ./
  ```
  where `<container_id>` can be retrieved through `docker ps` command


## References
[1] Afshin Taghavi Nasrabadi, Aliehsan Samiei, Anahita Mahzari, Ryan P.McMahan, Ravi Prakash, Mylène C. Q. Farias, and Marcelo M. Carvalho. *A taxonomy and dataset for 360° videos*. In Proceedings of the 10th ACM Multimedia Systems Conference, MMSys ’19, page 273–278, Amherst, Massachusetts, 2019.

[2] J. van der Hooft, S. Petrangeli, T. Wauters, R. Huysegems, P. R. Alface, T. Bostoen, and F. De Turck. *HTTP/2-Based Adaptive Streaming of HEVC Video Over 4G/LTE Networks*. IEEE Communications Letters, 20(11):2177–2180, 2016.

[3] Darijo Raca, Dylan Leahy, Cormac J. Sreenan, and Jason J. Quinlan. *Beyond
throughput, the next generation: A 5g dataset with channel and context metrics*. In Proceedings of the 11th ACM Multimedia Systems Conference, MMSys ’20, page 303–308, Istanbul, Turkey, 2020.

## Contact

For inquiries, please feel free to reach out to [Ehab Ghabashneh](mailto:eghabash@purdue.edu?subject=[Dragonfly]%20[Question]%20Docker%20Image)

