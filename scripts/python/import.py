
from gerbera import *
from os.path import relpath,dirname

#Add this line to use reverse-geocoding on pictures
#import reverse_geocode

def processCdsObject(obj,path):

    if obj.mimeType == 'audio/x-mpegurl' : return
    if obj.mimeType == 'audio/x-scpls' : return

    mime = obj.mimeType.split('/')[0]

    if (mime == 'audio') : addAudio(obj,path)
    if (mime == 'image') : addImage(obj,path)    
    if (mime == 'video') :
        if (obj.onlineservice == ONLINE_SERVICE_APPLE_TRAILERS):
            addTrailer(obj,path)
        else:
            addVideo(obj,path)

    # We now also have OGG Theora recognition, so we can ensure that
    # Vorbis
    if (obj.mimeType == 'application/ogg'):
        if (obj.theora == 1):
            addVideo(obj,path)
        else:
            addAudio(obj,path)


def addImage(obj,path):
	#By Date
	date = obj.meta[M_DATE]
	addCdsObject(obj,['Pictures', 'Date',date.split('-')[0] , date.split('-')[1]])

	#By Path
	dir = relpath(dirname(obj.location), path)
	if dir != '.':
		addCdsObject(obj,['Pictures', 'Directories'] + dir.split("/"))
    

	#By Location
    #uncommend to Enable reverse-geocoding
    #Compile with EXIV2 and add EXIF tags to config.xml to extract GPS metadata
	"""
    if "Exif.GPSInfo.GPSLatitude" in obj.aux:
		lat = [int(n) for n in obj.aux["Exif.GPSInfo.GPSLatitude"].replace(" ","/").split("/")]
		lat = lat[0]/lat[1] + lat[2]/lat[3]/60 + lat[4]/lat[5]/3600
		if obj.aux["Exif.GPSInfo.GPSLatitudeRef"] == "S": lat *= -1
		lng = [int(n) for n in obj.aux["Exif.GPSInfo.GPSLongitude"].replace(" ","/").split("/")]
		lng = lng[0]/lng[1] + lng[2]/lng[3]/60 + lng[4]/lng[5]/3600
		if obj.aux["Exif.GPSInfo.GPSLongitudeRef"] == "W": lng *= -1
		location = reverse_geocode.search([(lat,lng)])
		addCdsObject(obj, ["Pictures","Places",location[0]["country"],location[0]["city"]])
    """

def addVideo(obj,path):
	addCdsObject(obj, ['Video', 'All Video'], UPNP_CLASS_CONTAINER)

	#By Path
	d = relpath(dirname(obj.location), path)
	if d != '.':
		addCdsObject(obj, ['Video', 'Directories'] + d.split("/"),UPNP_CLASS_CONTAINER)


def addAudio(obj,path):
    
    # First we will gather all the metadata that is provided by our object

    # Note the difference between obj.title and obj.meta[M_TITLE] -
    # while object.title will originally be set to the file name,
    # obj.meta[M_TITLE] will contain the parsed title - in this
    # particular example the ID3 title of an MP3.

    title =  obj.meta.get(M_TITLE,obj.title);
    artist = obj.meta.get(M_ARTIST,'Unknown')
    album =  obj.meta.get(M_ALBUM,'Unknown')
    year =   obj.meta.get(M_DATE,'Unknown').split('-')[0] #Year ist first, so also unkown matches
    genre =  obj.meta.get(M_GENRE,'Unknown')
    
    composer =  obj.meta.get(M_COMPOSER,'None');
    conductor = obj.meta.get(M_CONDUCTOR,'None');
    orchestra = obj.meta.get(M_ORCHESTRA,'None');

    artist_full = obj.meta.get(M_ARTIST,None)
    album_full =  obj.meta.get(M_ALBUM,None)

    desc = [artist,album,title,year,genre]

    description = obj.meta.get(M_DESCRIPTION,",".join([a for a in desc if a != 'Unknown']));

# uncomment this if you want to have track numbers in front of the title in album view
#    track = ' ' + obj.meta.get(M_TRACKNUMBER,'').zfill(2)
# comment the following line out if you uncomment the stuff above  :)
    track = ''

    # uncomment this if you want to have channel numbers in front of the title

#    var channels = obj.res.get(R_NRAUDIOCHANNELS,'0')
#    if (channels == "1"): track = track + '[MONO]'
#    else if (channels == "2") : track = track + '[STEREO]'
#    else: track = track + '[MULTI]'

    obj.title = title;

    # The UPnP class argument to addCdsObject() is optional, if it is
    # not supplied the default UPnP class will be used. However, it
    # is suggested to correctly set UPnP classes of containers and
    # objects - this information may be used by some renderers to
    # identify the type of the container and present the content in a
    # different manner .

    addCdsObject(obj, ['Audio', 'All Audio'])
    addCdsObject(obj, ['Audio', 'Artists', artist, 'All Songs'])
       
    obj.title = ' - '.join(desc[:3]);
    addCdsObject(obj,['Audio', 'All - full name']);
    addCdsObject(obj,['Audio', 'Artists', artist, 'All - full name']);
    
    obj.title = track + title;
    addCdsObject(obj,  ['Audio', 'Artists', artist, album], UPNP_CLASS_CONTAINER_MUSIC_ALBUM);
    
    # Remember, the server will sort all items by ID3 track if the
    # container class is set to UPNP_CLASS_CONTAINER_MUSIC_ALBUM.
    addCdsObject(obj, ['Audio', 'Albums', album], UPNP_CLASS_CONTAINER_MUSIC_ALBUM);
    addCdsObject(obj, ['Audio', 'Genres', genre], UPNP_CLASS_CONTAINER_MUSIC_GENRE);
    addCdsObject(obj, ['Audio', 'Year', year]);
    addCdsObject(obj, ['Audio', 'Composers', composer], UPNP_CLASS_CONTAINER_MUSIC_COMPOSER);

def addTrailer(obj):
    # First we will add the item to the 'All Trailers' container, so
    # that we get a nice long playlist:
    addCdsObject(obj, ['Online Services', 'Apple Trailers', 'All Trailers'])

    # We also want to sort the trailers by genre, however we need to
    # take some extra care here: the genre property here is a comma
    # separated value list, so one trailer can have several matching
    # genres that will be returned as one string. We will split that
    # string and create individual genre containers.
    genre = obj.meta[M_GENRE];
    if genre: 
        # A genre string "Science Fiction, Thriller" will be split to
        # "Science Fiction" and "Thriller" respectively.
        for genres in genre.split(', '):
            addCdsObject(obj,  ['Online Services', 'Apple Trailers', 'Genres', genres]);
        

    # The release date is offered in a YYYY-MM-DD format, we won't do
    # too much extra checking regading validity, however we only want
    # to group the trailers by year and month
    reldate = obj.meta[M_DATE];
    if reldate and len(reldate) >= 7 :
        addCdsObject(obj, ['Online Services', 'Apple Trailers', 'Release Date', reldate[:7]])

    # We also want to group the trailers by the date when they were
    # originally posted, the post date is available via the aux
    # array. Similar to the release date, we will cut off the day and
    # create our containres in the YYYY-MM format.
    postdate = obj.aux[APPLE_TRAILERS_AUXDATA_POST_DATE];
    if postdate and len(postdate) >= 7:
        addCdsObject(obj,  ['Online Services', 'Apple Trailers', 'Post Date', postdate[:7]])
    
