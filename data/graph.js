//import { timeFormatDefaultLocale } from "d3-time-format";


var formatMillisecond = d3.timeFormat(".%L"),
  formatSecond = d3.timeFormat(":%S"),
  formatMinute = d3.timeFormat("%H:%M"),
  formatHour = d3.timeFormat("%H:%M"),
  formatDay = d3.timeFormat("%a %d"),
  formatWeek = d3.timeFormat("%b %d"),
  formatMonth = d3.timeFormat("%B"),
  formatYear = d3.timeFormat("%Y");

function multiFormat(date) {
  console.log('multiFormat', date);
  return (d3.timeSecond(date) < date ? formatMillisecond
    : d3.timeMinute(date) < date ? formatSecond
      : d3.timeHour(date) < date ? formatMinute
        : d3.timeDay(date) < date ? formatHour
          : d3.timeMonth(date) < date ? (d3.timeWeek(date) < date ? formatDay : formatWeek)
            : d3.timeYear(date) < date ? formatMonth
              : formatYear)(date);
};

class HistoricalChart {
  constructor() {
    this.margin;
    this.width;
    this.height;
    this.xScale;
    this.yScale;
    this.zoom;
    this.currentData = [];
    this.initialiseChart(plot);

    const viewRemote = document.querySelector('input[id=rem]');
    viewRemote.addEventListener('change', event => {
      this.toggleRemote(document.querySelector('input[id=rem]').checked);
    });

    const viewTarget = document.querySelector('input[id=target]');
    viewTarget.addEventListener('change', event => {
      this.toggleTarget(document.querySelector('input[id=target]').checked);
    });

    const viewTop = document.querySelector('input[id=top]');
    viewTop.addEventListener('change', event => {
      this.toggleTop(document.querySelector('input[id=top]').checked);
    });

    const viewBottom = document.querySelector('input[id=bot]');
    viewBottom.addEventListener('change', event => {
      this.toggleBottom(document.querySelector('input[id=bot]').checked);
    });

    const viewFront = document.querySelector('input[id=fnt]');
    viewFront.addEventListener('change', event => {
      this.toggleFront(document.querySelector('input[id=fnt]').checked);
    });

    const viewBack = document.querySelector('input[id=bck]');
    viewBack.addEventListener('change', event => {
      this.toggleBack(document.querySelector('input[id=bck]').checked);
    });

    const viewDiff = document.querySelector('input[id=diff]');
    viewDiff.addEventListener('change', event => {
      this.toggleDiff(document.querySelector('input[id=diff]').checked);
    });

    const viewHeaters = document.querySelector('input[id=htr]');
    viewHeaters.addEventListener('change', event => {
      this.toggleHeaters(document.querySelector('input[id=htr]').checked);
    });

    const viewTime = document.querySelector('select[id=time]');
    viewTime.addEventListener('change', event => {
      this.toggleTime(document.querySelector('select[id=time]').value);
    });
  }

  initialiseChart(data) {
    const nextYearStartDate = new Date();  // Now
    const thisYearStartDate = new Date(nextYearStartDate.getTime() - time_on_graph * 60 * 60 * 1000);

    // remove invalid data points
    const validData = data.filter(row => row['time'] && row['rem']);

    // filter out data based on time period
    this.currentData = validData.filter(row => {
      if (row['time']) {
        return (
          row['time'] >= thisYearStartDate.getTime() && row['time'] < nextYearStartDate.getTime()
        );
      }
    });

    const viewportWidth = Math.max(
      document.documentElement.clientWidth,
      window.innerWidth
    );
    const viewportHeight = Math.max(
      document.documentElement.clientHeight,
      window.innerHeight
    );
    this.margin = { top: 20, right: 40, bottom: 20, left: 40 };
    if (viewportWidth <= 768) {
      this.width = viewportWidth - this.margin.left - this.margin.right; // Use the window's width
      this.height = 0.7 * viewportHeight - this.margin.top - this.margin.bottom; // Use the window's height
    } else {
      this.width = viewportWidth - this.margin.left - this.margin.right - 80;
      this.height = viewportHeight - this.margin.top - this.margin.bottom - 80; // Use the window's height
    }
    // find data range
    var xMin = d3.min(this.currentData, d => d['time']);
    var xMax = d3.max(this.currentData, d => d['time']);
    var yMin = d3.min(this.currentData, d => d['rem']);
    var yMax = d3.max(this.currentData, d => d['rem']);
    var temp = d3.min(this.currentData, d => d['target']);
    if (temp < yMin) { yMin = temp }
    temp = d3.max(this.currentData, d => d['target']);
    if (temp > yMax) { yMax = temp }
    temp = d3.min(this.currentData, d => d['top']);
    if (temp < yMin) { yMin = temp }
    temp = d3.max(this.currentData, d => d['top']);
    if (temp > yMax) { yMax = temp }
    temp = d3.min(this.currentData, d => d['bot']);
    if (temp < yMin) { yMin = temp }
    temp = d3.max(this.currentData, d => d['bot']);
    if (temp > yMax) { yMax = temp }

    // scale using range
    this.xScale = d3
      .scaleTime()
      .domain([xMin, xMax])
      .range([0, this.width]);

    this.yScale = d3
      .scaleLinear()
      .domain([yMin, yMax])
      .range([this.height, 0]);

    // add chart SVG to the page
    const svg = d3
      .select('#chart')
      .append('svg')
      .attr('width', this.width + this.margin['left'] + this.margin['right'])
      .attr('height', this.height + this.margin['top'] + this.margin['bottom'])
      .append('g')
      .attr(
        'transform',
        `translate(${this.margin['left']}, ${this.margin['top']})`
      );

    // create the axes component
    this.xAxis = svg
      .append('g')
      .attr('class', 'xAxis')
      .attr('transform', `translate(0, ${this.height})`)
      .call(d3.axisBottom()
        .tickFormat(() => console.log("t"))
        .scale(this.xScale)
        //.tickFormat(function(d){return multiFormat(d);})
        //.tickFormat(d => multiFormat(d))
        //.tickFormat(d3.timeFormat("%H:%M:%S"))
        //.tickFormat(d => d + "%")
      );

    //d3.axisBottom().tickFormat(() => '');
    d3.axisBottom().ticks(4);

    this.yAxis = svg
      .append('g')
      .attr('class', 'yAxis')
      .attr('transform', `translate(${this.width}, 0)`)
      .call(d3.axisRight(this.yScale));
    svg
      .append('g')
      .attr('id', 'leftAxis')
      .attr('transform', `translate(0, 0)`);

    // define x and y crosshair properties
    const focus = svg
      .append('g')
      .attr('class', 'focus')
      .style('display', 'none');

    focus.append('circle').attr('r', 4.5);
    focus.append('line').classed('x', true);
    focus.append('line').classed('y', true);

    svg
      .append('rect')
      .attr('class', 'overlay')
      .attr('width', this.width)
      .attr('height', this.height);

    d3.select('.overlay')
      .style('fill', 'none')
      .style('pointer-events', 'all');

    d3.selectAll('.focus line')
      .style('fill', 'none')
      .style('stroke', '#67809f')
      .style('stroke-width', '1.5px')
      .style('stroke-dasharray', '3 3');

    svg
      .append('clipPath')
      .attr('id', 'clip')
      .append('rect')
      .attr('width', this.width)
      .attr('height', this.height);

    // group volume series bar charts, and with clip-path attribute
    svg
      .append('g')
      .attr('id', 'volume-series')
      .attr('clip-path', 'url(#clip)');

    // generates the rest of the graph
    this.updateChart();

    /* Handle zoom and pan */
    this.zoom = d3
      .zoom()
      .scaleExtent([1, 10])
      .translateExtent([[0, 0], [this.width, this.height]]) // pan limit
      .extent([[0, 0], [this.width, this.height]]) // zoom limit
      .on('zoom', (d, i, nodes) => this.zoomed(d, i, nodes));

    d3.select('svg').call(this.zoom);
  }

  zoomed(d, i, nodes) {
    const xAxis = d3.axisBottom(this.xScale);
    const yAxis = d3.axisRight(this.yScale);
    const tickWidth = 5;
    const bodyWidth = 5;
    // only fire the zoomed function when an actual event is triggered, rather than on every update
    if (d3.event.sourceEvent || d3.event.transform.k !== 1) {
      // create new scale ojects based on zoom/pan event
      const updatedXScale = d3.event.transform.rescaleX(this.xScale);
      const updatedYScale = d3.event.transform.rescaleY(this.yScale);
      // update axes
      const xMin = d3.min(this.currentData, d => d['time']);
      const xMax = d3.max(this.currentData, d => d['time']);
      const xRescale = d3
        .scaleTime()
        .domain([xMin, xMax])
        .range([0, this.width]);
      this.xScale.domain(d3.event.transform.rescaleX(xRescale).domain());
      this.yAxis.call(yAxis.scale(updatedYScale));
      this.xAxis.call(xAxis.scale(updatedXScale));
      // update close price and moving average lines based on zoom/pan
      const updateClosePriceChartPlot = d3
        .line()
        .x(d => updatedXScale(d['time']))
        .y(d => updatedYScale(d['rem']));
      const updateMovingAverageLinePlot = d3
        .line()
        .x(d => updatedXScale(d['time']))
        .y(d => updatedYScale(d['target']))
        .curve(d3.curveBasis);

      d3.select('.price-chart').attr('d', updateClosePriceChartPlot);
      d3.select('.moving-average-line').attr('d', updateMovingAverageLinePlot);

      // update volume series based on zoom/pan
      d3.selectAll('.vol').attr('x', d => updatedXScale(d['time']));

      // update crosshair position on zooming/panning
      const overlay = d3.select('.overlay');
      const focus = d3.select('.focus');
      const bisectDate = d3.bisector(d => d.date).left;

      // remove old crosshair
      overlay.exit().remove();

      // enter, and update the attributes
      overlay
        .enter()
        .append('g')
        .attr('class', 'focus')
        .style('display', 'none');

      overlay
        .attr('class', 'overlay')
        .attr('width', this.width)
        .attr('height', this.height)
        .on('mouseover', () => focus.style('display', null))
        .on('mouseout', () => focus.style('display', 'none'))
        .on('mousemove', (d, i, nodes) => {
          const correspondingDate = updatedXScale.invert(d3.mouse(nodes[i])[0]);
          //gets insertion point
          const i1 = bisectDate(this.currentData, correspondingDate, 1);
          const d0 = this.currentData[i1 - 1];
          const d1 = this.currentData[i1];
          const currentPoint =
            correspondingDate - d0['time'] > d1['time'] - correspondingDate
              ? d1
              : d0;
          focus.attr(
            'transform',
            `translate(${updatedXScale(currentPoint['time'])}, ${updatedYScale(
              currentPoint['rem']
            )})`
          );

          focus
            .select('line.x')
            .attr('x1', 0)
            .attr('x2', this.width - updatedXScale(currentPoint['time']))
            .attr('y1', 0)
            .attr('y2', 0);

          focus
            .select('line.y')
            .attr('x1', 0)
            .attr('x2', 0)
            .attr('y1', 0)
            .attr('y2', this.height - updatedYScale(currentPoint['rem']));

          this.updateLegends(currentPoint);
          this.updateSecondaryLegends(currentPoint['time']);
        });
    }
  }


  // Start of update

  setDataset(event) {
    const nextYearStartDate = new Date();
    const thisYearStartDate = new Date(nextYearStartDate.getTime() - time_on_graph * 60 * 60 * 1000);

    // remove invalid data points
    const validData = plot.filter(row => row['time'] && row['rem']);

    this.currentData = validData.filter(row => {
      if (row['time']) {
        return (
          row['time'] >= thisYearStartDate.getTime() && row['time'] < nextYearStartDate.getTime()
        );
      }
    });

    const viewportWidth = Math.max(document.documentElement.clientWidth, window.innerWidth);
    const viewportHeight = Math.max(document.documentElement.clientHeight, window.innerHeight);
    if (viewportWidth <= 768) {
      this.width = viewportWidth - this.margin.left - this.margin.right; // Use the window's width
      this.height = 0.5 * viewportHeight - this.margin.top - this.margin.bottom; // Use the window's height
    } else {
      this.width = viewportWidth - this.margin.left - this.margin.right - 80;
      this.height = viewportHeight - this.margin.top - this.margin.bottom - 80; // Use the window's height
    }

    /* update the min, max values, and scales for the axes */
    var xMin = d3.min(this.currentData, d => Math.min(d['time']));
    var xMax = d3.max(this.currentData, d => Math.max(d['time']));
    var yMin = 20;
    var yMax = 20;
    var yTmp;

    if (document.querySelector('input[id=rem]').checked) {
      yTmp = d3.min(this.currentData, d => Math.min(d['rem']));
      if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => Math.max(d['rem']));
      if (yTmp > yMax) { yMax = yTmp }
    }
    if (document.querySelector('input[id=target]').checked) {
      yTmp = d3.min(this.currentData, d => d['target']);
      if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => d['target']);
      if (yTmp > yMax) { yMax = yTmp }
    }
    if (document.querySelector('input[id=top]').checked) {
      yTmp = d3.min(this.currentData, d => d['top']);
      if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => d['top']);
      if (yTmp > yMax) { yMax = yTmp }
    }
    if (document.querySelector('input[id=bot]').checked) {
      yTmp = d3.min(this.currentData, d => d['bot']);
      if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => d['bot']);
      if (yTmp > yMax) { yMax = yTmp }
    }
    if (document.querySelector('input[id=fnt]').checked) {
      yTmp = d3.min(this.currentData, d => d['fnt']);
      if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => d['fnt']);
      if (yTmp > yMax) { yMax = yTmp }
    }
    if (document.querySelector('input[id=bck]').checked) {
      yTmp = d3.min(this.currentData, d => d['bck']);
      if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => d['bck']);
      if (yTmp > yMax) { yMax = yTmp }
    }
    if (document.querySelector('input[id=diff]').checked) {
      yTmp = d3.min(this.currentData, d => d['diff']);
      if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => d['diff']);
      if (yTmp > yMax) { yMax = yTmp }
    }
    if (document.querySelector('input[id=htr]').checked) {
      if (yMin > 0) { yMin = 0 }
      //yTmp = d3.min(this.currentData, d => d['htr']);
      //if (yTmp < yMin) { yMin = yTmp }
      yTmp = d3.max(this.currentData, d => d['htr']);
      if (yTmp > yMax) { yMax = yTmp }
    }

    this.xScale.domain([xMin, xMax]);
    this.yScale.domain([yMin, yMax]);

    this.updateChart();
  }

  updateChart() {
    /* Update the axes */
    d3.select('.xAxis').call(d3.axisBottom(this.xScale));
    d3.select('.yAxis').call(d3.axisRight(this.yScale));

    /* updating of crosshair */
    this.updateCrosshairProperties();

    /* Update the volume series bar chart */
    //    this.renderVolumeBarCharts();

    /* Update the price chart */
    const closeCheckboxToggle = document.querySelector('input[id=rem]').checked;
    this.toggleRemote(closeCheckboxToggle);

    /* Update the moving average line */
    const movingAverageCheckboxToggle = document.querySelector('input[id=target]').checked;
    this.toggleTarget(movingAverageCheckboxToggle);

    /* Update the top temperature line */
    const topCheckboxToggle = document.querySelector('input[id=top]').checked;
    this.toggleTop(topCheckboxToggle);

    /* Update the bottom temperature line */
    const bottomCheckboxToggle = document.querySelector('input[id=bot]').checked;
    this.toggleBottom(bottomCheckboxToggle);

    /* Update the bottom temperature line */
    const frontCheckboxToggle = document.querySelector('input[id=fnt]').checked;
    this.toggleFront(frontCheckboxToggle);

    /* Update the bottom temperature line */
    const diffCheckboxToggle = document.querySelector('input[id=diff]').checked;
    this.toggleDiff(diffCheckboxToggle);

    /* Update the bottom temperature line */
    const backCheckboxToggle = document.querySelector('input[id=bck]').checked;
    this.toggleBack(backCheckboxToggle);

    /* Update the heaters line */
    const heatersCheckboxToggle = document.querySelector('input[id=htr]').checked;
    this.toggleHeaters(heatersCheckboxToggle);
  }

  /* Mouseover function to generate crosshair */
  generateCrosshair(current) {
    //returns corresponding value from the domain
    const focus = d3.select('.focus');
    const bisectDate = d3.bisector(d => d['time']).left;
    const correspondingDate = this.xScale.invert(d3.mouse(current)[0]);
    //gets insertion point
    const i = bisectDate(this.currentData, correspondingDate, 1);
    const d0 = this.currentData[i - 1];
    const d1 = this.currentData[i];
    const currentPoint =
      correspondingDate - d0['time'] > d1['time'] - correspondingDate ? d1 : d0;
    focus.attr(
      'transform',
      `translate(${this.xScale(currentPoint['time'])}, ${this.yScale(
        currentPoint['rem']
      )})`
    );

    focus
      .select('line.x')
      .attr('x1', 0)
      .attr('x2', this.width - this.xScale(currentPoint['time']))
      .attr('y1', 0)
      .attr('y2', 0);

    focus
      .select('line.y')
      .attr('x1', 0)
      .attr('x2', 0)
      .attr('y1', 0)
      .attr('y2', this.height - this.yScale(currentPoint['rem']));

    // updates the legend to display the date, open, close, high, low, and volume and selected mouseover area
    this.updateLegends(currentPoint);
  }

  updateLegends(currentPoint) {
    d3.selectAll('.primary-legend').remove();
    const legendKeys = Object.keys(currentPoint);
    const lineLegendSelect = d3
      .select('#chart')
      .select('g')
      .selectAll('.primary-legend')
      .data(legendKeys);
    lineLegendSelect.join(
      enter =>
        enter
          .append('g')
          .attr('class', 'primary-legend')
          .attr('transform', (d, i) => `translate(0, ${i * 20})`)
          .append('text')
          .text(d => {
            if (d === 'time') {
              return `${d}: ${new Date(currentPoint[d]).toLocaleString()}`;
            } else if (
              d === 'high' ||
              d === 'low' ||
              d === 'open' ||
              d === 'rem'
            ) {
              return `${d}: ${currentPoint[d].toFixed(2)}`;
            } else {
              return `${d}: ${currentPoint[d]}`;
            }
          })
          .style('font-size', '0.8em')
          .style('fill', 'black')
          .attr('transform', 'translate(15,9)') //align texts with boxes*/
    );
  }

  updateSecondaryLegends(currentDate) {
    const secondaryLegend = {};
    const secondaryLegendKeys = Object.keys(secondaryLegend);

    d3.selectAll('.secondary-legend').remove();
    if (secondaryLegendKeys.length > 0) {
      const secondaryLegendSelect = d3
        .select('#chart')
        .select('g')
        .selectAll('.secondary-legend')
        .data(secondaryLegendKeys);
      secondaryLegendSelect.join(
        enter =>
          enter
            .append('g')
            .attr('class', 'secondary-legend')
            .attr('transform', (d, i) => `translate(0, ${i * 20})`)
            .append('text')
            .text(d => { })
            .style('font-size', '0.8em')
            .style('fill', 'white')
            .attr('transform', 'translate(150,9)'),

        exit => exit.remove()
      );
    }
  }

  updateCrosshairProperties() {
    // select the existing crosshair, and bind new data
    const overlay = d3.select('.overlay');
    const focus = d3.select('.focus');

    // remove old crosshair
    overlay.exit().remove();

    // enter, and update the attributes
    overlay
      .enter()
      .append('g')
      .attr('class', 'focus')
      .style('display', 'none');

    overlay
      .attr('class', 'overlay')
      .attr('width', this.width)
      .attr('height', this.height)
      .on('mouseover', () => focus.style('display', null))
      .on('mouseout', () => focus.style('display', 'none'))
      .on('mousemove', (d, i, nodes) => this.generateCrosshair(nodes[i]));
  }

  renderVolumeBarCharts() {
    const chart = d3.select('#chart').select('g');
    //const yMinVolume = d3.min(this.currentData, d => Math.min(d['volume']));
    //const yMaxVolume = d3.max(this.currentData, d => Math.max(d['volume']));

    //const yVolumeScale = d3
    //.scaleLinear()
    //.domain([yMinVolume, yMaxVolume])
    //.range([this.height, this.height * (3 / 4)]);
    //d3.select('#leftAxis').call(d3.axisLeft(yVolumeScale));

    //select, followed by join
    const bars = d3
      .select('#volume-series')
      .selectAll('.vol')
      .data(this.currentData, d => d['time']);

    bars.join(
      enter =>
        enter
          .append('rect')
          .attr('class', 'vol')
          .attr('x', d => this.xScale(d['time']))
          .attr('y', d => yVolumeScale(d['volume']))
          .attr('fill', (d, i) => {
            if (i === 0) {
              return '#03a678';
            } else {
              // green bar if price is rising during that period, and red when price is falling
              return this.currentData[i - 1].close > d.close
                ? '#c0392b'
                : '#03a678';
            }
          })
          .attr('width', 1)
          .attr('height', d => this.height - yVolumeScale(d['volume'])),
      update =>
        update
          .transition()
          .duration(750)
          .attr('x', d => this.xScale(d['time']))
          .attr('y', d => yVolumeScale(d['volume']))
          .attr('fill', (d, i) => {
            if (i === 0) {
              return '#03a678';
            } else {
              // green bar if price is rising during that period, and red when price is falling
              return this.currentData[i - 1].close > d.close
                ? '#c0392b'
                : '#03a678';
            }
          })
          .attr('width', 1)
          .attr('height', d => this.height - yVolumeScale(d['volume']))
    );
  }

  toggleRemote(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          //.transition()
          //.duration(750)
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const line = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['rem']));
      const lineSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.price-chart')
        .data([this.currentData]);

      lineSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'price-chart')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'red')
            .attr('stroke-width', '1')
            .attr('d', line),
        update =>
          update
            //.transition()
            //.duration(750)
            .attr('d', line)
      );
    } else {
      // Remove close price chart
      d3.select('.price-chart').remove();
    }
  }

  toggleTarget(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          //.transition()
          //.duration(750)
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const movingAverageLine = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['target']))
        .curve(d3.curveBasis);
      const movingAverageSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.moving-average-line')
        .data([this.currentData]);

      movingAverageSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'moving-average-line')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'fuchsia')
            .attr('stroke-width', '1')
            .attr('d', movingAverageLine),
        update =>
          update
            //.transition()
            //.duration(750)
            .attr('d', movingAverageLine)
      );
    } else {
      // Remove moving average line
      d3.select('.moving-average-line').remove();
    }
  }

  toggleTop(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          //.transition()
          //.duration(750)
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const line = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['top']))
        .curve(d3.curveBasis);
      const lineSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.top-line')
        .data([this.currentData]);

      lineSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'top-line')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'gold')
            .attr('stroke-width', '1')
            .attr('d', line),
        update =>
          update
            .attr('d', line)
      );
    } else {
      // Remove moving average line
      d3.select('.top-line').remove();
    }
  }

  toggleBottom(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const line = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['bot']))
        .curve(d3.curveBasis);
      const lineSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.bottom-line')
        .data([this.currentData]);

      lineSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'bottom-line')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'goldenrod')
            .attr('stroke-width', '1')
            .attr('d', line),
        update =>
          update
            .attr('d', line)
      );
    } else {
      // Remove moving average line
      d3.select('.bottom-line').remove();
    }
  }

  toggleFront(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const line = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['fnt']))
        .curve(d3.curveBasis);
      const lineSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.front-line')
        .data([this.currentData]);

      lineSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'front-line')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'lightgreen')
            .attr('stroke-width', '1')
            .attr('d', line),
        update =>
          update
            .attr('d', line)
      );
    } else {
      // Remove moving average line
      d3.select('.front-line').remove();
    }
  }

  toggleBack(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const line = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['bck']))
        .curve(d3.curveBasis);
      const lineSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.back-line')
        .data([this.currentData]);

      lineSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'back-line')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'palegreen')
            .attr('stroke-width', '1')
            .attr('d', line),
        update =>
          update
            .attr('d', line)
      );
    } else {
      // Remove moving average line
      d3.select('.back-line').remove();
    }
  }

  toggleDiff(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const line = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['diff']))
        .curve(d3.curveBasis);
      const lineSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.diff-line')
        .data([this.currentData]);

      lineSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'diff-line')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'lightpink')
            .attr('stroke-width', '1')
            .attr('d', line),
        update =>
          update
            .attr('d', line)
      );
    } else {
      // Remove moving average line
      d3.select('.diff-line').remove();
    }
  }

  toggleHeaters(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          .call(this.zoom.transform, d3.zoomIdentity.scale(1));
      }

      const line = d3
        .line()
        .x(d => this.xScale(d['time']))
        .y(d => this.yScale(d['htr']))
      //.curve(d3.curveBasis);
      const lineSelect = d3
        .select('#chart')
        .select('svg')
        .select('g')
        .selectAll('.heaters-line')
        .data([this.currentData]);

      lineSelect.join(
        enter =>
          enter
            .append('path')
            .style('fill', 'none')
            .attr('class', 'heaters-line')
            .attr('clip-path', 'url(#clip)')
            .attr('stroke', 'lightpink')
            .attr('stroke-width', '1')
            .attr('d', line),
        update =>
          update
            .attr('d', line)
      );
    } else {
      // Remove moving average line
      d3.select('.heaters-line').remove();
    }
  }

  toggleTime(value) {
    switch (value) {
      case '1h':
        time_on_graph = 1;
        break;
      case '2h':
        time_on_graph = 2;
        break;
      case '6h':
        time_on_graph = 6;
        break;
      case '12h':
        time_on_graph = 12;
        break;
      case '24h':
        time_on_graph = 24;
        break;
      case '2d':
        time_on_graph = 2 * 24;
        break;
      case '3h':
        time_on_graph = 3 * 24;
        break;
      case '4d':
        time_on_graph = 4 * 24;
        break;
      case '5d':
        time_on_graph = 5 * 24;
        break;
      case '10d':
        time_on_graph = 10 * 24;
        break;
      case '30d':
        time_on_graph = 30 * 24;
        break;
      default:
        return;
    }
    plot = [];
    //console.log(time_on_graph);
    //retrieve();
    //this.setDataset(0);
  }
}

var time_on_graph = 6;

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var update;

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function onOpen(event) {
  console.log('Connection opened');
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
  update = JSON.parse(event.data);
  addSnapshot(update);
  //console.log(event.data);
  retrieve();
}

window.addEventListener('load', onLoad);

function onLoad(event) {
  initWebSocket();
  openDb();
  sleep(1000).then(() => { retrieve(); });
}

const DB_NAME = 'Snapshots';
const DB_VERSION = 1;
const DB_STORE_NAME = 'snapshots';

var db;

function openDb() {
  console.log("openDb ...");
  var request = indexedDB.open(DB_NAME, DB_VERSION);

  request.onsuccess = function (evt) {
    // Equal to: db = request.result;
    db = this.result;
    console.log("openDb DONE");
  };

  request.onerror = function (evt) {
    console.error("openDb:", evt.target.errorCode);
  };

  request.onupgradeneeded = function (evt) {
    console.log("openDb.onupgradeneeded");
    var store = evt.currentTarget.result.createObjectStore(DB_STORE_NAME, { keyPath: 'time' });
  };
}

function getObjectStore(store_name, mode) {
  var tx = db.transaction(store_name, mode);
  return tx.objectStore(store_name);
}

function addSnapshot(obj) {
  //console.log("addSnapshot arguments:", arguments);

  var store = getObjectStore(DB_STORE_NAME, 'readwrite');
  var req;

  try {
    req = store.put(obj);
  } catch (e) {
    if (e.name == 'DataCloneError')
      displayActionFailure("This engine doesn't know how to clone a Blob, " +
        "use Firefox");
    throw e;
  }
  req.onsuccess = function (evt) {
    //  console.log("Insertion in DB successful");
  };
  req.onerror = function () {
    console.error("addSnapshot error", this.error);
  };
}

let plot = [];

function retrieve() {
  const high = getTimestampInSeconds();
  const low = high - time_on_graph * 60 * 60;
  const boundKeyRange = IDBKeyRange.bound(low, high, false, true);

  const transaction = db.transaction([DB_STORE_NAME], "readonly");
  const objectStore = transaction.objectStore(DB_STORE_NAME);

  objectStore.openCursor(boundKeyRange).onsuccess = (event) => {
    const cursor = event.target.result;
    var record;
    if (cursor) {
      record = cursor.value;
      record.time *= 1000;
      record.htr = (8 * record.one_pwr + 12 * record.two_pwr) / 4;
      record.diff = record.bot - record.top;
      if (plot.length == 0) {
        plot.push(record);
      } else {
        let temp = plot.slice(-1);
        //      console.log( "temp", temp );
        //      console.log( "temp[0].time", temp[0].time );
        //      console.log( "record.time", record.time );

        if (temp[0].time < record.time) {
          //        console.log( "temp[0].time < record.time", temp[0].time < record.time );
          plot.push(record);
        }
      }
      cursor.continue();
    }
  }
  //console.log("plot", plot);

  chart.setDataset(0);
}

function getTimestampInSeconds() {
  return Math.floor(Date.now() / 1000)
}
