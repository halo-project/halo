#!/usr/bin/env python3

# NOTE: system dependencies are:
# sudo apt install \
#   python3 \
#   python3-pandas \
#   python3-click \
#   python3-seaborn \

import click
import os
import read_minibench as rm
import seaborn as sns
import matplotlib
import matplotlib.pyplot as plt

output_dir="./"

def export_fig(g, filename, extraArtists=[]):
    fig = g.get_figure()
    fig.savefig(os.path.join(output_dir, filename + ".pdf"), bbox_extra_artists=extraArtists, bbox_inches='tight')
    plt.close(fig)


def configure_seaborn():
  ''' stylistic configuration of seaborn '''
  # There are five preset seaborn themes: darkgrid, whitegrid, dark, white, and ticks
  # https://seaborn.pydata.org/tutorial/aesthetics.html
  sns.set_style("whitegrid")

  # request TrueType fonts and not Type 3.
  # src: http://phyletica.org/matplotlib-fonts/
  matplotlib.rcParams['pdf.fonttype'] = 42
  matplotlib.rcParams['ps.fonttype'] = 42

  # set font for all text
  # currentStyle = sns.axes_style()
  # fontName="Arial" # 'Linux Biolinum O', 'Arial'
  # currentStyle['font.sans-serif'] = [fontName]
  # sns.set_style(currentStyle)

  # size of labels, scaled for: paper, notebook, talk, poster in smallest -> largest
  sns.set_context("talk")


def extrapolate_iters(df):
  '''
  to save time, we setup the benchmark suite to not iterate per-trial
  for programs compiled without halo running. to show a nice plot though,
  we want those observations, so we duplicate the rows for those observations
  for other iterations in the df
  '''

  maxIter = df['iter'].max()
  for kind in df['flags'].unique():
    for it in range(2,maxIter+1):
      subset = df[df['flags'] == kind]

      if len(subset[subset['iter'] == it]) != 0:
        continue

      # grab all the observations from the previous iteration
      prev_iter = it-1
      prev_iter_rows = subset[subset['iter'] == prev_iter].copy()

      assert len(prev_iter_rows) != 0, "cannot extrapolate!"

      # forward them as this iteration
      prev_iter_rows['iter'] = it
      df = df.append(prev_iter_rows, ignore_index=True)

  return df


def plot_progression(df, title, file_prefix):
  g = sns.lineplot(x='iter', y='time', hue='flags', data=df, legend='brief')

  # https://stackoverflow.com/questions/51579215/remove-seaborn-lineplot-legend-title?rq=1
  handles, labels = g.get_legend_handles_labels()

  # https://matplotlib.org/3.1.1/tutorials/intermediate/legend_guide.html
  # https://stackoverflow.com/questions/30490740/move-legend-outside-figure-in-seaborn-tsplot
  lgd = plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc='lower left',
           ncol=3, mode="expand", borderaxespad=0., title="",
           handles=handles[1:], labels=labels[1:])

  # https://stackoverflow.com/questions/10101700/moving-matplotlib-legend-outside-of-the-axis-makes-it-cutoff-by-the-figure-box
  export_fig(g, file_prefix, [lgd])


def plot_iter_progressions(df):
  ''' produces multiple plots showing tuning progression over time '''
  df = df.copy()

  programs = set(df['program'])
  aot_opts = set(df['aot_opt'])
  for prog in programs:
    for opt in aot_opts:
      obs = df[(df['program'] == prog) & (df['aot_opt'] == opt)]
      obs = extrapolate_iters(obs)
      title = prog + "_" + opt
      plot_progression(obs, title, title)



@click.command()
@click.argument('csv_filename')
@click.option("--dir", default="./", type=str,
               help="Output directory for the plots")
def main(csv_filename, dir):
  global output_dir
  output_dir = dir

  configure_seaborn()

  pd = rm.read_csv(csv_filename)
  plot_iter_progressions(pd)



if __name__ == '__main__':
    main() # ignore the linter here.
