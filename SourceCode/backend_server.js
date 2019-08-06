var express = require('express');
var http = require('http');
var app = express();
var fs = require("fs");
var request = require("request");
var _ = require("underscore");

//System Status Flags
var bd_rsrvd = 0;
var bd_uname1 = "empty";
var bd_uname2 = "empty";
var bd_uname3 = "empty";
var bd_drlck = 0;
var bd_clnrqst = 0;
var bd_mtnrqst = 0;

var tp_lvlok = 0;
var tp_clnrqst = 0;

var sprayrqst = 0;

//Allow CORS
app.use(function(req, res, next) {
	  res.header("Access-Control-Allow-Origin", "*");
	  res.header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
	  next();
});

//Returns Main Bathroom Door Unit Status
app.get('/mainbd/status', function (req, res) {
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Main BD Request 
app.get('/mainbd/request', function (req, res) {
  if (bd_drlck != 1) bd_rsrvd = 1;
  
  if (bd_uname1 == "empty"){
    bd_uname1 = req.query.bd_uname;
  } else if (bd_uname2 == "empty") {
    bd_uname2 = req.query.bd_uname;
  } else if (bd_uname3 == "empty") { 
    bd_uname3 = req.query.bd_uname;    
  }
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Cancel reservation
app.get('/mainbd/cnclrsrv', function (req, res) {
  if(bd_uname2 != "empty") bd_rsrvd = 1; else bd_rsrvd = 0;
  bd_uname1 = bd_uname2;//shift username here
  bd_uname2 = bd_uname3;//shift username here
  bd_uname3 = "empty";//shift username here
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Door Lock
app.get('/mainbd/drlck', function (req, res) {
  bd_drlck = 1;
  bd_rsrvd = 0;
  if(bd_uname1 == "empty"){
      bd_uname1 = "USERNAME";
  }    
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Door unlock
app.get('/mainbd/drunlck', function (req, res) {
  bd_drlck = 0;
  if(bd_uname2 != "empty") bd_rsrvd = 1; else bd_rsrvd = 0;
  bd_uname1 = bd_uname2;//shift username here
  bd_uname2 = bd_uname3;//shift username here
  bd_uname3 = "empty";//shift username here    
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Set clean request flag
app.get('/mainbd/clnrqst', function (req, res) {
  bd_clnrqst = 1;
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Clean request clear
app.get('/mainbd/clnclr', function (req, res) {
  bd_clnrqst = 0;
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Maintenance Request
app.get('/mainbd/mntcrqst', function (req, res) {
  bd_mtnrqst = 1;
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})

//Maintenance Request Clear
app.get('/mainbd/mntcclr', function (req, res) {
  bd_mtnrqst = 0;
  var response = "bd_rsrvd:"+bd_rsrvd+",bd_uname1:"+bd_uname1+",bd_uname2:"+bd_uname2+",bd_uname3:"+bd_uname3+",bd_drlck:"+bd_drlck+",bd_clnrqst:"+bd_clnrqst+",bd_mtnrqst:"+bd_mtnrqst;
  res.end(response);
})


//Toilet Paper Unit Status
app.get('/tp/status', function (req, res) {
  var response = "tp_lvlok:"+tp_lvlok+",tp_rqst:"+tp_clnrqst;
  res.end(response);

})

//Toilet Paper Level OK Set
app.get('/tp/lvlokset', function (req, res) {
  tp_lvlok = 1;
  var response = "tp_lvlok:"+tp_lvlok+",tp_rqst:"+tp_clnrqst;
  res.end(response);

})

//Toilet Paper Level OK Set
app.get('/tp/lvlokreset', function (req, res) {
  tp_lvlok = 0;
  var response = "tp_lvlok:"+tp_lvlok+",tp_rqst:"+tp_clnrqst;
  res.end(response);

})

//Toilet Paper Request Set
app.get('/tp/setrqst', function (req, res) {
  tp_clnrqst = 1;
  var response = "tp_lvlok:"+tp_lvlok+",tp_rqst:"+tp_clnrqst;
  res.end(response);

})

//Toilet Paper Request Clear
app.get('/tp/clrrqst', function (req, res) {
  tp_clnrqst = 0;
  var response = "tp_lvlok:"+tp_lvlok+",tp_rqst:"+tp_clnrqst;
  res.end(response);

})

//Airfreshener Spray
app.get('/af/spray', function (req, res) {
  sprayrqst = 1;
  var response = "sprayrqst:"+sprayrqst;
  res.end(response);
})

//Airfreshener Spray Status
app.get('/af/sprayStat', function (req, res) {
  var response = "sprayrqst:"+sprayrqst;
  res.end(response);

})

//Airfreshener Spray Clear
app.get('/af/sprayClr', function (req, res) {
  sprayrqst = 0;
  var response = "sprayrqst:"+sprayrqst;
  res.end(response);
})


var server = app.listen(8071, function () {
   var host = server.address().address
   var port = server.address().port
   console.log("Example got demo listening at http://%s:%s", host, port)
})