//elements
var gurl = gId('curlUrl'); 
var szX = gId("sizeX"); 
var szY = gId("sizeY");
var szDiv = gId("sizeDiv"); 
var prw = gId("preview");
var sID = gId('segID');
var JLD = gId('JSONled');
var tSg = gId('targetSegment');
var brgh = gId("brightnessNumber");

var seDiv = gId("getSegmentsDiv")
var cjb = gId("copyJSONledbutton");
var frm = gId("formatSelector");
var cLN = gId("colorLimitNumber");
var haIDe = gId("haID");
var haUe = gId("haUID");
var haNe = gId("haName");
var aS = gId("addressingSelector");
var cFS = gId("colorFormatSelector");
var lSS  = gId("ledSetupSelector");
var imin = gId('image-info');
var imcn = gId('image-container');
var bcn = gId("button-container");
var im = gId('image');
//var ss = gId("sendSvgP");
var scDiv = gId("scaleDiv");
var w = window;
var canvas = gId('pixelCanvas');
var brgV = gId("brightnessValue");
var cLV = gId("colorLimitValue")

//vars
var httpArray = [];
var fileJSON = '';

var hideableRows = d.querySelectorAll(".ha-hide");
for (var i = 0; i < hideableRows.length; i++) {
  hideableRows[i].classList.add("hide");
}

var accentColor = '#eee';
var accentTextColor = '#777';
var prsCol = '#ccc';
var greenColor = '#056b0a';
var redColor = '#6b050c';

var scaleToggleOffd = "M17,7H7A5,5 0 0,0 2,12A5,5 0 0,0 7,17H17A5,5 0 0,0 22,12A5,5 0 0,0 17,7M7,15A3,3 0 0,1 4,12A3,3 0 0,1 7,9A3,3 0 0,1 10,12A3,3 0 0,1 7,15Z";
var scaleToggleOnd = "M17,7H7A5,5 0 0,0 2,12A5,5 0 0,0 7,17H17A5,5 0 0,0 22,12A5,5 0 0,0 17,7M17,15A3,3 0 0,1 14,12A3,3 0 0,1 17,9A3,3 0 0,1 20,12A3,3 0 0,1 17,15Z";

var sSg = gId("getSegmentsSVGpath");