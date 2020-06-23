#!/usr/bin/env python3

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


def plot_progression(df, title, file_prefix):
  g = sns.lineplot(x='iter', y='time', hue='flags', data=df)

  # g.set_
  # g.set_ylabels("Running time (sec)")

  export_fig(g, file_prefix)


def plot_iter_progressions(df):
  df = df.copy()

  programs = set(df['program'])
  aot_opts = set(df['aot_opt'])
  for prog in programs:
    for opt in aot_opts:
      obs = df[(df['program'] == prog) & (df['aot_opt'] == opt)]
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
