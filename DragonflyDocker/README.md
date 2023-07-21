# Dragonfly Image

Docker image to build and evaluate Dragonfly video streaming system.

## Image info

This image includes:

- Dragonfly, Pano, Flare, Two Tier source codes.
- 7 videos (tiled and pre-encoded into multiple qualities)
- 30 User trajectories.
- 21 Bandwidth traces.
- Scripts to regenerate results from our SIGCOMM '23 paper.

## Prerequisite

- Install Docker engine
  - For MACOSX users: run `brew cask install docker`
  - For Ubuntu users: run `sudo apt install docker.io && sudo snap install docker`
  - For Windows users: please refer to [Docker documentation](https://docs.docker.com/desktop/install/windows-install/#install-docker-desktop-on-windows)

## Download And Run The Image

- To **Download** the latest Dragonfly image, run the command below or you can refer to [this dockerhub repo](https://hub.docker.com/r/eghabash/dfly/tags) to track/download old versions of the image.
  ```
  docker pull eghabash/dfly
  ```
- To **Run** the image, run the following command
  ```
  docker run --cap-add=NET_ADMIN --privileged  --device /dev/net/tun:/dev/net/tun -i -t eghabash/dfly:0.1
  ```
  Couple of notes,

* If you are using remote linux machine, we recommend running the image inside [screen](https://linux.die.net/man/1/screen) shell.
* In case of unresposive shell, you can run `docker exec -it <container-id> bash` to gain access back to docker's shell. To retrieve `<container-id>`, please run `docker ps` command.

## Build And Run Dragonfly

Once you run the image, then:

- Run `cd home/dfly && ./bash.sh` to (i) download and build all dependecies, and (ii) build Dragonfly code. Please note that **dfly password** is `1` (long live security bugs!!)

### Evaluate Dragonfly

To reproduce evaluation results from [our paper](https://doi.org/10.1145/3603269.3604876), it takes more than a week of conducting experiements (175 hours to be precise). Therefore, we suggest first to evaluate with smaller sets of traces and users for which we provide the scripts.

**Caveat**: While the estimated experiment time for the smaller dataset is much less of approximately 6 hours, the outcome of this evaluation may not be an identical to these from the full dataset. Yet, we expect the patterns of both datasets to coincide.

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

  **Output:** This will plot all figures following the paper numberings, and save them under the following directory

  ```
  /home/dfly/plot_figures_result_partial/figs_partial/Figure_<number>_partial.png
  ```

  Likewise for the **full dataset**, except that you should replace the `partial` word with `full` for the above commands and path

- To **copy figures from docker**, please run:
  ```
  docker cp <container_id>:/home/dfly/plot_figures_result_partial/figs_partial/ ./
  ```
  where `<container_id>` can be retrieved through `docker ps` command

## Contact

For inquiries, please feel free to reach out to [Ehab Ghabashneh](mailto:eghabash@purdue.edu)
