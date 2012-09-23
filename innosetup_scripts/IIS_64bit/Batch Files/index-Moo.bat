REM create index.html
echo ^<!doctype html^>^

^<html lang="en" ^>^

 ^<head^>^

  ^<meta charset="utf-8" /^>^

  ^<meta name="author" content="Ruven Pillay &lt;ruven@users.sourceforge.netm&gt;"/^>^

  ^<meta name="keywords" content="IIPImage HTML5 Ajax IIP Zooming Streaming High Resolution Mootools"/^>^

  ^<meta name="description" content="IIPImage: High Resolution Remote Image Streaming Viewer"/^>^

  ^<meta name="copyright" content="&copy; 2003-2011 Ruven Pillay"/^>^

  ^<meta name="viewport" content="width=device-width; initial-scale=1.0; maximum-scale=1.0; user-scalable=0;" /^>^

  ^<meta name="apple-mobile-web-app-capable" content="yes" /^>^

  ^<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" /^>^

  ^<meta http-equiv="X-UA-Compatible" content="IE=9" /^>^

  ^<link rel="stylesheet" type="text/css" media="all" href="MooViewer2.0/css/iip.css" /^>^

^<!--[if lt IE 9]^>^

  ^<link rel="stylesheet" type="text/css" media="all" href="MooViewer2.0/css/ie.css" /^>^

^<![endif]--^>^

  ^<link rel="shortcut icon" href="MooViewer2.0/images/iip-favicon.png" /^>^

  ^<link rel="apple-touch-icon" href="MooViewer2.0/images/iip.png" /^>^

  ^<title^>IIPMooViewer 2.0 :: IIPImage High Resolution HTML5 Ajax Image Streaming Viewer^</title^>^

  ^<script type="text/javascript" src="MooViewer2.0/javascript/mootools-core-1.3.2-full-nocompat.js"^>^</script^>^

  ^<script type="text/javascript" src="MooViewer2.0/javascript/mootools-more-1.3.2.1.js"^>^</script^>^

  ^<script type="text/javascript" src="MooViewer2.0/javascript/protocols.js"^>^</script^>^

  ^<script type="text/javascript" src="MooViewer2.0/javascript/iipmooviewer-2.0.js"^>^</script^>^

^<!--[if lt IE 7]^>^

  ^<script src="http://ie7-js.googlecode.com/svn/version/2.1(beta4)/IE7.js"^>IE7_PNG_SUFFIX = ".png";^</script^>^

^<![endif]--^>^

  ^<script type="text/javascript"^>^

      function getImage() {^

      var searchString = window.location.search.substring(1),^

        i, val, params = searchString.split("&");^

      for (i=0;i^<params.length;i++) {^

        val = params[i].split("=");^

        if (val[0] == "image") {^

          return val[1];^

        }^

      }^

      return "demo.jp2";^

    }^

    // The iipsrv server path (/fcgi-bin/iipsrv.fcgi by default)^

    var server = 'http://localhost/%~1/iipsrv.fcgi';^

    // The *full* image path on the server. This path does *not* need to be in the web^

    // server root directory. On Windows, use Unix style forward slash paths without^

    // the "c:" prefix^

    var images = getImage();^

    // Copyright or information message^

    var credit = '^&copy; copyright or information message';^

    // Create our viewer object^

    // See documentation for more details of options^

    var iipmooviewer = new IIPMooViewer( "viewer", {^

        image: images,^

        server: server,^

        credit: credit,^

        scale: 20.0,^

        showNavWindow: true,^

        showNavButtons: true,^

        winResize: true,^

        protocol: 'iip',^

        prefix: 'MooViewer2.0/images/'^

    });^

  ^</script^>^

  ^<style type="text/css"^>^

    body{ height: 100%%; }^

    div#viewer{ width: 100%%; height: 100%%; }^

  ^</style^>^

 ^</head^>^

 ^<body^>^

   ^<div id="viewer"^>^</div^>^

  ^</body^>^

^</html^> > %2\index.html
REM Create url file for viewing images
echo [InternetShortcut]^

URL=http://localhost/%~1/index.html > "%~2\IIPImage MooViewer 2.0.url"
REM Complete configuration.ini file for Jpeg2000Transcoder
echo [OutputFolder]^

name=%3^

[IIPImageURL]^

url="http://localhost/%~1/index.html" >> %4\configuration.ini
REM Open test image in browser (command may have problems with virt. directory with space in name)
start http://localhost/%~1/index.html
