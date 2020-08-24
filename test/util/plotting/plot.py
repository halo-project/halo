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

def export_fig(g, filename, legendFig=None, extraArtists=[], lineplot=True):
    if lineplot:
      fig = g.get_figure()
    else:
      fig = g.fig

    if legendFig:
      legendFig.savefig(os.path.join(output_dir, filename + "_legend.pdf"), bbox_inches='tight')
    fig.savefig(os.path.join(output_dir, filename + ".pdf"), bbox_extra_artists=extraArtists, bbox_inches='tight')
    plt.close(fig)


def configure_seaborn(bars):
  ''' stylistic configuration of seaborn '''
  # There are five preset seaborn themes: darkgrid, whitegrid, dark, white, and ticks
  # https://seaborn.pydata.org/tutorial/aesthetics.html
  # also turn on ticks with
  sns.set_style("whitegrid", rc={"xtick.bottom" : True, "ytick.left" : True})

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
  if bars:
    sns.set_context("notebook")

  else:
    sns.set_context("notebook")
    # override some sizes
    # https://stackoverflow.com/questions/43670164/font-size-of-axis-labels-in-seaborn?rq=1
    matplotlib.rcParams["axes.titlesize"] = 14
    matplotlib.rcParams["axes.labelsize"] = 14
    matplotlib.rcParams["xtick.labelsize"] = 14
    matplotlib.rcParams["ytick.labelsize"] = 14
    matplotlib.rcParams["lines.linewidth"] = 3


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


def plot_progression(df, title, file_prefix, baseline, palette_name):
  df = df.copy()

  df, did_normalize = do_normalize(df, baseline)

  hueCol = 'flags'
  numHues = len(df[hueCol].unique())

  if palette_name == "cubehelix":
    palette = sns.cubehelix_palette(numHues)
  else:
    palette = sns.color_palette(palette_name, numHues)

  # NOTE: for individual lines per trial, you can use:
  #   units='trial', estimator=None, lw=1,
  g = sns.lineplot(x='iter', y='time', hue=hueCol, data=df, legend='brief',
                   palette=palette)

  ylab = "Speedup" if did_normalize else "Time"
  g.set(ylabel=ylab, xlabel="Tuning Iterations")

  # Y AXIS LIMITS
  buffer = 0.1
  yMin = round(min(float(min(df['time'])), 0.5), 2) - buffer
  yMax = round(max(float(max(df['time'])), 1.5), 2) + buffer
  plt.ylim(yMin, yMax)
  g.yaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(2))


  xMin = min(df['iter'])
  xMax = max(df['iter'])
  plt.xlim(xMin-1, xMax+1)
  g.xaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(2))

  # https://stackoverflow.com/questions/51579215/remove-seaborn-lineplot-legend-title?rq=1
  handles, labels = g.get_legend_handles_labels()

  # https://matplotlib.org/3.1.1/tutorials/intermediate/legend_guide.html
  # https://stackoverflow.com/questions/30490740/move-legend-outside-figure-in-seaborn-tsplot
  lgd = plt.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc='lower left',
           ncol=4, mode="expand", borderaxespad=0., title='',
           handles=handles[1:], labels=labels[1:], prop={'size': 14})

  # https://stackoverflow.com/questions/10101700/moving-matplotlib-legend-outside-of-the-axis-makes-it-cutoff-by-the-figure-box
  export_fig(g, file_prefix, None, [lgd])


def plot_iter_progressions(df, baseline, palette):
  ''' produces multiple plots showing tuning progression over time '''
  df = df.copy()

  programs = set(df['program'])
  aot_opts = set(df['aot_opt'])
  for prog in programs:
    for opt in aot_opts:
      obs = df[(df['program'] == prog) & (df['aot_opt'] == opt)]
      obs = extrapolate_iters(obs)
      title = prog + "_" + opt
      plot_progression(obs, prog, title, baseline, palette)

# https://stackoverflow.com/questions/28931224/adding-value-labels-on-a-matplotlib-bar-chart
def add_value_labels(ax, spacing=5):
    """Add labels to the end of each bar in a bar chart.

    Arguments:
        ax (matplotlib.axes.Axes): The matplotlib object containing the axes
            of the plot to annotate.
        spacing (int): The distance between the labels and the bars.
    """

    # For each bar: Place a label
    for rect in ax.patches:
        # Get X and Y placement of label from rect.
        y_value = rect.get_height()
        x_value = rect.get_x() + rect.get_width() / 2

        # Number of points between bar and label. Change to your liking.
        space = spacing
        # Vertical alignment for positive values
        va = 'bottom'

        # If value of bar is negative: Place label below bar
        if y_value < 0:
            # Invert space to place label below
            space *= -1
            # Vertically align label at top
            va = 'top'

        # Use Y value as label and format number with one decimal place
        label = "{:.2f}".format(y_value)

        # Create annotation
        ax.annotate(
            label,                      # Use `label` as label
            (x_value, y_value),         # Place label at end of the bar
            fontsize=8,
            xytext=(0, space),          # Vertically shift label by `space`
            textcoords="offset points", # Interpret `xytext` as offset in points
            ha='center',                # Horizontally center label
            va=va)                      # Vertically align label differently for
                                        # positive and negative values.




def plot_bar_chart(df, baseline, palette_name):
  df = df.copy()

  df, did_normalize = do_normalize(df, baseline)

  hueCol = 'flags'
  numHues = len(df[hueCol].unique())

  if palette_name == "cubehelix":
    palette = sns.cubehelix_palette(numHues)
  else:
    palette = sns.color_palette(palette_name, numHues)

  g = sns.catplot(data=df, kind='bar', x='program', y='time',
                  hue=hueCol, palette=palette, errwidth=1.125, capsize=0.0625,
                  legend_out=False)

  yMin = 0.5
  yMax = 1.5

  plt.ylim(yMin, yMax)

  axis = g.axes.flat[0]  # i think this is y axis?? whatever. it works
  add_value_labels(axis)

  leg = axis.get_legend()
  new_title = ''
  leg.set_title(new_title)

  yaxis_label = "Speedup" if did_normalize else "Time"
  g.set_axis_labels(x_var="", y_var=yaxis_label)

  plt.xticks(rotation=20)

  export_fig(g, "bars", None, [], lineplot=False)


def do_normalize(df, baseline):
  ''' returns new dataframe and whether it was normalized or not '''

  if len(baseline) == 0:
    return (df, False)

  for prog, progGrp in df.groupby('program'):
    for it, iterGrp in progGrp.groupby('iter'):
      baselineVal = iterGrp[iterGrp['flags'] == baseline]['time'].mean()

      # compute speed-up: old-time / new-time
      temp = iterGrp['time'].apply(lambda x: baselineVal / x)

      df.loc[(df['program'] == prog) & (df['iter'] == it), 'time'] = temp

  return (df, True)


@click.command()
@click.argument('csv_filename')
@click.option("--dir", default="./", type=str,
               help="Output directory for the plots")
@click.option("--exclude", default="default,halomon", type=str,
               help="Exclude some flag configurations from plots")
@click.option("--baseline", default="aot", type=str,
               help="Normalize the data relative to a flag configuration")
@click.option("--palette", default="cubehelix", type=str,
               help="Name of the color palette")
@click.option("--bars", default=False, type=bool,
               help="Generate cumulative average bar chart instead of line plots.")
def main(csv_filename, dir, exclude, baseline, palette, bars):
  global output_dir
  output_dir = dir

  os.makedirs(output_dir, exist_ok=True)

  excluded = set(exclude.split(","))

  configure_seaborn(bars)

  pd = rm.read_csv(csv_filename)

  # drop data for the excluded flags
  for flag in excluded:
    pd = pd[pd['flags'] != flag]

  if bars:
    plot_bar_chart(pd, baseline, palette)
  else:
    plot_iter_progressions(pd, baseline, palette)



if __name__ == '__main__':
    main() # ignore the linter here.
