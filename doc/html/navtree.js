var NAVTREE =
[
  [ "iipsrv", "index.html", [
    [ "Class List", "annotated.html", [
      [ "Cache", "classCache.html", null ],
      [ "CNT", "classCNT.html", null ],
      [ "CVT", "classCVT.html", null ],
      [ "DeepZoom", "classDeepZoom.html", null ],
      [ "Environment", "classEnvironment.html", null ],
      [ "FCGIWriter", "classFCGIWriter.html", null ],
      [ "FIF", "classFIF.html", null ],
      [ "FileWriter", "classFileWriter.html", null ],
      [ "HEI", "classHEI.html", null ],
      [ "ICC", "classICC.html", null ],
      [ "iip_destination_mgr", "structiip__destination__mgr.html", null ],
      [ "IIPImage", "classIIPImage.html", null ],
      [ "IIPResponse", "classIIPResponse.html", null ],
      [ "JPEGCompressor", "classJPEGCompressor.html", null ],
      [ "JTL", "classJTL.html", null ],
      [ "JTLS", "classJTLS.html", null ],
      [ "KakaduImage", "classKakaduImage.html", null ],
      [ "kdu_stream_message", "classkdu__stream__message.html", null ],
      [ "LYR", "classLYR.html", null ],
      [ "Memcache", "classMemcache.html", null ],
      [ "OBJ", "classOBJ.html", null ],
      [ "QLT", "classQLT.html", null ],
      [ "RawTile", "classRawTile.html", null ],
      [ "RGN", "classRGN.html", null ],
      [ "SDS", "classSDS.html", null ],
      [ "Session", "structSession.html", null ],
      [ "SHD", "classSHD.html", null ],
      [ "SPECTRA", "classSPECTRA.html", null ],
      [ "Task", "classTask.html", null ],
      [ "TIL", "classTIL.html", null ],
      [ "TileManager", "classTileManager.html", null ],
      [ "Timer", "classTimer.html", null ],
      [ "Tokenizer", "classTokenizer.html", null ],
      [ "TPTImage", "classTPTImage.html", null ],
      [ "View", "classView.html", null ],
      [ "Watermark", "classWatermark.html", null ],
      [ "WID", "classWID.html", null ],
      [ "Writer", "classWriter.html", null ],
      [ "Zoomify", "classZoomify.html", null ]
    ] ],
    [ "Class Index", "classes.html", null ],
    [ "Class Hierarchy", "hierarchy.html", [
      [ "Cache", "classCache.html", null ],
      [ "Environment", "classEnvironment.html", null ],
      [ "FCGIWriter", "classFCGIWriter.html", null ],
      [ "FileWriter", "classFileWriter.html", null ],
      [ "iip_destination_mgr", "structiip__destination__mgr.html", null ],
      [ "IIPImage", "classIIPImage.html", [
        [ "KakaduImage", "classKakaduImage.html", null ],
        [ "TPTImage", "classTPTImage.html", null ]
      ] ],
      [ "IIPResponse", "classIIPResponse.html", null ],
      [ "JPEGCompressor", "classJPEGCompressor.html", null ],
      [ "kdu_stream_message", "classkdu__stream__message.html", null ],
      [ "Memcache", "classMemcache.html", null ],
      [ "RawTile", "classRawTile.html", null ],
      [ "Session", "structSession.html", null ],
      [ "Task", "classTask.html", [
        [ "CNT", "classCNT.html", null ],
        [ "CVT", "classCVT.html", null ],
        [ "DeepZoom", "classDeepZoom.html", null ],
        [ "FIF", "classFIF.html", null ],
        [ "HEI", "classHEI.html", null ],
        [ "ICC", "classICC.html", null ],
        [ "JTL", "classJTL.html", null ],
        [ "JTLS", "classJTLS.html", null ],
        [ "LYR", "classLYR.html", null ],
        [ "OBJ", "classOBJ.html", null ],
        [ "QLT", "classQLT.html", null ],
        [ "RGN", "classRGN.html", null ],
        [ "SDS", "classSDS.html", null ],
        [ "SHD", "classSHD.html", null ],
        [ "SPECTRA", "classSPECTRA.html", null ],
        [ "TIL", "classTIL.html", null ],
        [ "WID", "classWID.html", null ],
        [ "Zoomify", "classZoomify.html", null ]
      ] ],
      [ "TileManager", "classTileManager.html", null ],
      [ "Timer", "classTimer.html", null ],
      [ "Tokenizer", "classTokenizer.html", null ],
      [ "View", "classView.html", null ],
      [ "Watermark", "classWatermark.html", null ],
      [ "Writer", "classWriter.html", null ]
    ] ],
    [ "Class Members", "functions.html", null ],
    [ "File List", "files.html", [
      [ "Cache.h", null, null ],
      [ "ColourTransforms.h", null, null ],
      [ "DSOImage.h", null, null ],
      [ "Environment.h", null, null ],
      [ "IIPImage.h", null, null ],
      [ "IIPResponse.h", null, null ],
      [ "jinclude.h", null, null ],
      [ "JPEGCompressor.h", null, null ],
      [ "KakaduImage.h", null, null ],
      [ "Memcached.h", null, null ],
      [ "RawTile.h", null, null ],
      [ "Task.h", null, null ],
      [ "TileManager.h", null, null ],
      [ "Timer.h", null, null ],
      [ "Tokenizer.h", null, null ],
      [ "TPTImage.h", null, null ],
      [ "View.h", null, null ],
      [ "Watermark.h", null, null ],
      [ "Writer.h", null, null ]
    ] ],
    [ "Directories", "dirs.html", [
      [ "src", "dir_e4ba62853a0524747fa15764b26367c8.html", null ]
    ] ]
  ] ]
];

function createIndent(o,domNode,node,level)
{
  if (node.parentNode && node.parentNode.parentNode)
  {
    createIndent(o,domNode,node.parentNode,level+1);
  }
  var imgNode = document.createElement("img");
  if (level==0 && node.childrenData)
  {
    node.plus_img = imgNode;
    node.expandToggle = document.createElement("a");
    node.expandToggle.href = "javascript:void(0)";
    node.expandToggle.onclick = function() 
    {
      if (node.expanded) 
      {
        $(node.getChildrenUL()).slideUp("fast");
        if (node.isLast)
        {
          node.plus_img.src = node.relpath+"ftv2plastnode.png";
        }
        else
        {
          node.plus_img.src = node.relpath+"ftv2pnode.png";
        }
        node.expanded = false;
      } 
      else 
      {
        expandNode(o, node, false);
      }
    }
    node.expandToggle.appendChild(imgNode);
    domNode.appendChild(node.expandToggle);
  }
  else
  {
    domNode.appendChild(imgNode);
  }
  if (level==0)
  {
    if (node.isLast)
    {
      if (node.childrenData)
      {
        imgNode.src = node.relpath+"ftv2plastnode.png";
      }
      else
      {
        imgNode.src = node.relpath+"ftv2lastnode.png";
        domNode.appendChild(imgNode);
      }
    }
    else
    {
      if (node.childrenData)
      {
        imgNode.src = node.relpath+"ftv2pnode.png";
      }
      else
      {
        imgNode.src = node.relpath+"ftv2node.png";
        domNode.appendChild(imgNode);
      }
    }
  }
  else
  {
    if (node.isLast)
    {
      imgNode.src = node.relpath+"ftv2blank.png";
    }
    else
    {
      imgNode.src = node.relpath+"ftv2vertline.png";
    }
  }
  imgNode.border = "0";
}

function newNode(o, po, text, link, childrenData, lastNode)
{
  var node = new Object();
  node.children = Array();
  node.childrenData = childrenData;
  node.depth = po.depth + 1;
  node.relpath = po.relpath;
  node.isLast = lastNode;

  node.li = document.createElement("li");
  po.getChildrenUL().appendChild(node.li);
  node.parentNode = po;

  node.itemDiv = document.createElement("div");
  node.itemDiv.className = "item";

  node.labelSpan = document.createElement("span");
  node.labelSpan.className = "label";

  createIndent(o,node.itemDiv,node,0);
  node.itemDiv.appendChild(node.labelSpan);
  node.li.appendChild(node.itemDiv);

  var a = document.createElement("a");
  node.labelSpan.appendChild(a);
  node.label = document.createTextNode(text);
  a.appendChild(node.label);
  if (link) 
  {
    a.href = node.relpath+link;
  } 
  else 
  {
    if (childrenData != null) 
    {
      a.className = "nolink";
      a.href = "javascript:void(0)";
      a.onclick = node.expandToggle.onclick;
      node.expanded = false;
    }
  }

  node.childrenUL = null;
  node.getChildrenUL = function() 
  {
    if (!node.childrenUL) 
    {
      node.childrenUL = document.createElement("ul");
      node.childrenUL.className = "children_ul";
      node.childrenUL.style.display = "none";
      node.li.appendChild(node.childrenUL);
    }
    return node.childrenUL;
  };

  return node;
}

function showRoot()
{
  var headerHeight = $("#top").height();
  var footerHeight = $("#nav-path").height();
  var windowHeight = $(window).height() - headerHeight - footerHeight;
  navtree.scrollTo('#selected',0,{offset:-windowHeight/2});
}

function expandNode(o, node, imm)
{
  if (node.childrenData && !node.expanded) 
  {
    if (!node.childrenVisited) 
    {
      getNode(o, node);
    }
    if (imm)
    {
      $(node.getChildrenUL()).show();
    } 
    else 
    {
      $(node.getChildrenUL()).slideDown("fast",showRoot);
    }
    if (node.isLast)
    {
      node.plus_img.src = node.relpath+"ftv2mlastnode.png";
    }
    else
    {
      node.plus_img.src = node.relpath+"ftv2mnode.png";
    }
    node.expanded = true;
  }
}

function getNode(o, po)
{
  po.childrenVisited = true;
  var l = po.childrenData.length-1;
  for (var i in po.childrenData) 
  {
    var nodeData = po.childrenData[i];
    po.children[i] = newNode(o, po, nodeData[0], nodeData[1], nodeData[2],
        i==l);
  }
}

function findNavTreePage(url, data)
{
  var nodes = data;
  var result = null;
  for (var i in nodes) 
  {
    var d = nodes[i];
    if (d[1] == url) 
    {
      return new Array(i);
    }
    else if (d[2] != null) // array of children
    {
      result = findNavTreePage(url, d[2]);
      if (result != null) 
      {
        return (new Array(i).concat(result));
      }
    }
  }
  return null;
}

function initNavTree(toroot,relpath)
{
  var o = new Object();
  o.toroot = toroot;
  o.node = new Object();
  o.node.li = document.getElementById("nav-tree-contents");
  o.node.childrenData = NAVTREE;
  o.node.children = new Array();
  o.node.childrenUL = document.createElement("ul");
  o.node.getChildrenUL = function() { return o.node.childrenUL; };
  o.node.li.appendChild(o.node.childrenUL);
  o.node.depth = 0;
  o.node.relpath = relpath;

  getNode(o, o.node);

  o.breadcrumbs = findNavTreePage(toroot, NAVTREE);
  if (o.breadcrumbs == null)
  {
    o.breadcrumbs = findNavTreePage("index.html",NAVTREE);
  }
  if (o.breadcrumbs != null && o.breadcrumbs.length>0)
  {
    var p = o.node;
    for (var i in o.breadcrumbs) 
    {
      var j = o.breadcrumbs[i];
      p = p.children[j];
      expandNode(o,p,true);
    }
    p.itemDiv.className = p.itemDiv.className + " selected";
    p.itemDiv.id = "selected";
    $(window).load(showRoot);
  }
}

