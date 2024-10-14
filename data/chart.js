class HistoricalChart {
  constructor() {
    this.margin;
    this.width;
    this.height;
    this.xScale;
    this.yscale;
    this.zoom;
    this.currentData = [];
    //this.loadData('vig').then(data => { this.initialiseChart(data); });
    this.initialiseChart(plot);

    const selectElement = document.getElementById('select-stock');
    selectElement.addEventListener('change', event => {
      this.setDataset(event);
    });

    const viewClose = document.querySelector('input[id=close]');
    viewClose.addEventListener('change', event => {
      this.toggleClose(document.querySelector('input[id=close]').checked);
    });
  }

  loadData(selectedDataset = 'vig') {
    let loadFile = '';
    if (selectedDataset === 'vig') {
      loadFile = 'sample-data-vig.json';
    } else if (selectedDataset === 'vti') {
      loadFile = 'sample-data-vti.json';
    } else if (selectedDataset === 'vea') {
      loadFile = 'sample-data-vea.json';
    }

    return d3.json(loadFile).then(data => {
      const chartResultsData = data['chart']['result'][0];
      const quoteData = chartResultsData['indicators']['quote'][0];

      return {
        quote: chartResultsData['timestamp'].map((time, index) => ({
          date: new Date(time * 1000),
          high: quoteData['high'][index],
          low: quoteData['low'][index],
          open: quoteData['open'][index],
          close: quoteData['close'][index],
          volume: quoteData['volume'][index]
        }))
      };
    });
  }

  initialiseChart(data) {
    const nextYearStartDate = new Date();
    const thisYearStartDate = new Date(nextYearStartDate.getTime()-24*60*60*1000);

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
      this.width = viewportWidth - this.margin.left - this.margin.right;
      this.height = 0.75 * viewportHeight - this.margin.top - this.margin.bottom; // Use the window's height
    }

    // find data range
    const xMin = d3.min(this.currentData, d => d['time']);
    const xMax = d3.max(this.currentData, d => d['time']);
    const yMin = d3.min(this.currentData, d => d['rem']);
    const yMax = d3.max(this.currentData, d => d['rem']);

    // scale using range
    this.xScale = d3
      .scaleTime()
      .domain([xMin, xMax])
      .range([0, this.width]);

    this.yScale = d3
      .scaleLinear()
      .domain([yMin - 5, yMax + 4])
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
      .call(d3.axisBottom(this.xScale));

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

      d3.select('.price-chart').attr('d', updateClosePriceChartPlot);

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
              currentPoint['close']
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
      this.loadData("vig").then(response => {

        const nextYearStartDate = new Date();
        const thisYearStartDate = new Date(nextYearStartDate.getTime()-24*60*60*1000);

        //console.log("setDataset:")
        //console.log(plot);

        // remove invalid data points
        const validData = plot.filter(row => row['time'] && row['rem']);

        this.currentData = validData.filter(row => {
          if (row['time']) {
            return (
              row['time'] >= thisYearStartDate.getTime() && row['time'] < nextYearStartDate.getTime()
            );
          }
        });

        //console.log(validData);


        const viewportWidth = Math.max(
          document.documentElement.clientWidth,
          window.innerWidth
        );
        const viewportHeight = Math.max(
          document.documentElement.clientHeight,
          window.innerHeight
        );
        if (viewportWidth <= 768) {
          this.width = viewportWidth - this.margin.left - this.margin.right; // Use the window's width
          this.height =
            0.5 * viewportHeight - this.margin.top - this.margin.bottom; // Use the window's height
        } else {
          this.width =
            0.75 * viewportWidth - this.margin.left - this.margin.right;
          this.height = viewportHeight - this.margin.top - this.margin.bottom; // Use the window's height
        }

        /* update the min, max values, and scales for the axes */
        const xMin = d3.min(this.currentData, d => Math.min(d['time']));
        const xMax = d3.max(this.currentData, d => Math.max(d['time']));
        const yMin = d3.min(this.currentData, d => Math.min(d['rem']));
        const yMax = d3.max(this.currentData, d => Math.max(d['rem']));

        //console.log("xMin xMax yMin yMax", xMin, xMax, yMin, yMax);

        this.xScale.domain([xMin, xMax]);
        this.yScale.domain([yMin - 5, yMax + 4]);

        this.updateChart();
      });

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
//  const closeCheckboxToggle = document.querySelector('input[id=close]')
//    .checked;
    this.toggleClose(true);
  }

  /* Mouseover function to generate crosshair */
  generateCrosshair(current) {
    //returns corresponding value from the domain
    const focus = d3.select('.focus');
    const bisectDate = d3.bisector(d => d.date).left;
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
      .attr('x2', this.width - this.xScale(currentPoint['date']))
      .attr('y1', 0)
      .attr('y2', 0);

    focus
      .select('line.y')
      .attr('x1', 0)
      .attr('x2', 0)
      .attr('y1', 0)
      .attr('y2', this.height - this.yScale(currentPoint['close']));

    // updates the legend to display the date, open, close, high, low, and volume and selected mouseover area
    //this.updateLegends(currentPoint);
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
              return `${d}: ${currentPoint[d].toLocaleDateString()}`;
            } else if (
              d === 'high' ||
              d === 'low' ||
              d === 'open' ||
              d === 'close'
            ) {
              return `${d}: ${currentPoint[d].toFixed(2)}`;
            } else {
              return `${d}: ${currentPoint[d]}`;
            }
          })
          .style('font-size', '0.8em')
          .style('fill', 'white')
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
//    .on('mousemove', (d, i, nodes) => this.generateCrosshair(nodes[i]));
  }

  renderVolumeBarCharts() {
    const chart = d3.select('#chart').select('g');
    const yMinVolume = d3.min(this.currentData, d => Math.min(d['volume']));
    const yMaxVolume = d3.max(this.currentData, d => Math.max(d['volume']));

    const yVolumeScale = d3
      .scaleLinear()
      .domain([yMinVolume, yMaxVolume])
      .range([this.height, this.height * (3 / 4)]);
    //d3.select('#leftAxis').call(d3.axisLeft(yVolumeScale));

    //select, followed by join
    const bars = d3
      .select('#volume-series')
      .selectAll('.vol')
      .data(this.currentData, d => d['date']);

    bars.join(
      enter =>
        enter
          .append('rect')
          .attr('class', 'vol')
          .attr('x', d => this.xScale(d['date']))
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
          .attr('x', d => this.xScale(d['date']))
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

  toggleClose(value) {
    if (value) {
      if (this.zoom) {
        d3.select('svg')
          .transition()
          .duration(750)
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
             .attr('stroke', 'steelblue')
             .attr('stroke-width', '1.5')
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
}







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
    req = store.add(obj);
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
  const low = high - 24*60*60;
  const boundKeyRange = IDBKeyRange.bound(low, high, false, true);
//console.log("boundKeyRange", boundKeyRange);

  const transaction = db.transaction([DB_STORE_NAME], "readonly");
  const objectStore = transaction.objectStore(DB_STORE_NAME);

  objectStore.openCursor(boundKeyRange).onsuccess = (event) => {
    const cursor = event.target.result;
    var record;
    if (cursor) {
      record = cursor.value;
      record.time *= 1000;
      if( plot.length == 0 ) {
        plot.push(record);
      } else {
        let temp = plot.slice( -1 );
//      console.log( "temp", temp );
//      console.log( "temp[0].time", temp[0].time );
//      console.log( "record.time", record.time );

        if( temp[0].time < record.time ) {
//        console.log( "temp[0].time < record.time", temp[0].time < record.time );
          plot.push(record);
        }
      }
    cursor.continue();
    }
  }
//console.log("plot", plot);

  chart.setDataset(0);
  //	updateData()
}

function getTimestampInSeconds() {
  return Math.floor(Date.now() / 1000)
}
