# Congestion Control Project

This project is powered by HTSim. For more information about HTSim check the
[wiki](https://github.com/nets-cs-pub-ro/NDP/wiki).

We will be running the project using Jupyter Notebooks. They provide an
interactive Python environment which we will be using to quickly create plots.
We recommend the following
[guide](https://www.codecademy.com/article/how-to-use-jupyter-notebooks) on how
to use Jupyter. We will implement the congestion control algorithm in C++, whereas
Python is used for plotting.

> Don't forget to complete the file `team.secret`

## Running locally (strongly advised to use this approach)

This is the recommended way to run the project. You will need [jupyer lab](https://jupyter.org/install) on you pc.

```
sudo apt install python3-pip
pip3 install jupyterlab
```

To run the notebook:

```
cd sim
jupyter lab
# This will open a tab in your browser, open the Project_CongestionControl.ipynb notebook from there
```

## Running on Binder

You can run this online on
[![Binder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/git/https%3A%2F%2Fgitlab.cs.pub.ro%2Fpcom%2Fcongestion-control-project.git/main?labpath=sim%2FProject_CongestionControl.ipynb).
It will take around ~5 min to start your notebook. Don't forget to save the
changes you've made to the c files locally.
