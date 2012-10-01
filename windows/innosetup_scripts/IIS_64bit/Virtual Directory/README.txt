This folder includes url shortcuts, which can be called also from Start menu
and MooViewer for viewing images from IIPImage Server.

We are using IIPMooViewer version 2.0 beta, because it is out for more than a year,
(http://iipimage.sourceforge.net/2011/08/iipmooviewer-2-0-beta/),
but you can use another version, adding MooViewer folder here, then you must
take the file index.html out of MooViewer folder, edit it in appropriated way
and edit content of index-Moo.bat.

Content of index.html is modified during installation, in index-Moo.bat you can see,
how it is created (all is done because of url address, which can be changed by user in installer).
If you don't like this style, look into apache installer, it uses few different style of virtual
directories, where is fcgi-bin constant name for iipimage directory. You can update it that way,
if you don't want to create index in batch file.

Note that blank file iipsrv.fcgi is required.

IIPImage MooViewer 2.0.url is also in batch file index-Moo.bat.