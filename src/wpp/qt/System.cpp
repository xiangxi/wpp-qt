#include "System.h"

#include <QDebug>
#include <QNetworkConfiguration>
#include <QStandardPaths>
#include <QStringList>

#ifdef Q_OS_ANDROID
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <QtAndroid>
#endif
#include <QPainter>
#include <QDir>
#include <QTimeZone>


namespace wpp
{
namespace qt
{

System *System::singleton = 0;

System &System::getInstance()
{
	if ( singleton == 0 )
	{
		static System singletonInstance;
		singleton = &singletonInstance;
	}
	return *singleton;
}

#ifndef Q_OS_IOS
void System::initDeviceId()
{
#if defined(Q_OS_ANDROID)
	//final String deviceID = Secure.getString(getContentResolver(), Secure.ANDROID_ID);
	QAndroidJniObject Secure__ANDROID_ID = QAndroidJniObject::getStaticObjectField(
				"android/provider/Settings$Secure", "ANDROID_ID", "Ljava/lang/String;");
	qDebug() << __FUNCTION__ << "Secure__ANDROID_ID.isValid()=" << Secure__ANDROID_ID.isValid();

	QAndroidJniObject activity = QtAndroid::androidActivity();
	qDebug() << __FUNCTION__ << "activity.isValid()=" << activity.isValid();

	QAndroidJniObject contentResolver = activity.callObjectMethod("getContentResolver","()Landroid/content/ContentResolver;");
	qDebug() << __FUNCTION__ << "contentResolver.isValid()=" << contentResolver.isValid();

	QAndroidJniObject deviceIdJObj = QAndroidJniObject::callStaticObjectMethod(
				"android/provider/Settings$Secure", "getString",
				"(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
				contentResolver.object<jobject>(), Secure__ANDROID_ID.object<jstring>());
	qDebug() << __FUNCTION__ << "deviceIdJObj.isValid()=" << deviceIdJObj.isValid();

	this->deviceId = deviceIdJObj.toString();
	emit this->deviceIdChanged();
#endif

}
#endif

System::System()
	:
	  m_isDesktop(
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
		  true
#else
		  false
#endif
	),
	  m_isAndroid(
#ifdef Q_OS_ANDROID
		true
#else
		false
#endif
	),
	m_isIOS(
#ifdef Q_OS_IOS
		true
#else
		false
#endif
	),
	m_network("BearerNoNetwork"), hasNetwork(
#ifdef Q_OS_IOS
		false
#else
		false
#endif
	), slowNetwork(true),
	__IMPLEMENTATION_DETAIL_ENABLE_AUTO_ROTATE(false)

{
	qDebug() << "isAndroid:" << m_isAndroid;

	/*QList<QNetworkConfiguration> allConfigs = networkConfigurationManager.allConfigurations();
	qDebug() << "ALL network config count:" << allConfigs.count();

	QList<QNetworkConfiguration> activeConfigs = networkConfigurationManager.allConfigurations(QNetworkConfiguration::Active);
	qDebug() << "Active network config count:" << activeConfigs.count();
	QNetworkConfiguration conf = networkConfigurationManager.defaultConfiguration();
	qDebug() << "default conf bearer type:" << conf.bearerTypeName() ;*/

	qDebug() << "System(): hasNetwork:" << hasNetwork << ", networkConfigurationManager.isOnline(): " << networkConfigurationManager.isOnline();
//#ifdef Q_OS_IOS //iOS has not yet implemented QNetworkConfigurationManager
	//onNetworkOnlineStateChanged( true );
//#else
	onNetworkOnlineStateChanged( networkConfigurationManager.isOnline() );
//#endif

	//deviceId
	initDeviceId();


	connect(&networkConfigurationManager,SIGNAL(onlineStateChanged(bool)),this,SLOT(onNetworkOnlineStateChanged(bool)));
	connect(&networkConfigurationManager,SIGNAL(configurationChanged(QNetworkConfiguration)),this,SLOT(onNetworkConfigurationChanged(QNetworkConfiguration)));
	connect(&networkConfigurationManager,SIGNAL(updateCompleted()),this,SLOT(onNetworkConfigurationUpdateCompleted()));

//#ifndef Q_OS_IOS //iOS has not yet implemented QNetworkConfigurationManager
	networkConfigurationManager.updateConfigurations();
//#endif
}

void System::onNetworkOnlineStateChanged(bool isOnline)
{
	qDebug() << "System::onNetworkOnlineStateChanged(" << isOnline << ")";
	if ( !isOnline )
	{
		setHasNetwork(false);

		setNetwork("");
		setIsSlowNetwork(true);
	}
	else
	{
		setHasNetwork(true);

		QNetworkConfiguration networkConfig = networkConfigurationManager.defaultConfiguration();
		QNetworkConfiguration::BearerType bearerType = networkConfig.bearerType();
		qDebug() << "System::onNetworkOnlineStateChanged():default:" << networkConfig.bearerTypeName();
		setNetwork( networkConfig.bearerTypeName() );
		switch ( bearerType )
		{
		case QNetworkConfiguration::BearerUnknown:
			qDebug() << "QNetworkConfiguration::BearerUnknown";
			setIsSlowNetwork(true);
		break;
		case QNetworkConfiguration::BearerEthernet:
			qDebug() << "QNetworkConfiguration::BearerEthernet";
			setIsSlowNetwork(false);
		break;
		case QNetworkConfiguration::BearerWLAN:
			qDebug() << "QNetworkConfiguration::BearerWLAN";
			setIsSlowNetwork(false);
		break;
		case QNetworkConfiguration::Bearer2G:
			qDebug() << "QNetworkConfiguration::Bearer2G";
			setIsSlowNetwork(true);
		break;
		case QNetworkConfiguration::BearerCDMA2000:
			qDebug() << "QNetworkConfiguration::BearerCDMA2000";
			setIsSlowNetwork(true);
		break;
		case QNetworkConfiguration::BearerWCDMA:
			qDebug() << "QNetworkConfiguration::BearerWCDMA";
			setIsSlowNetwork(true);
		break;
		case QNetworkConfiguration::BearerHSPA:
			qDebug() << "QNetworkConfiguration::BearerHSPA";
			setIsSlowNetwork(true);
		break;
		case QNetworkConfiguration::BearerBluetooth:
			qDebug() << "QNetworkConfiguration::BearerBluetooth";
			setIsSlowNetwork(true);
		break;
		case QNetworkConfiguration::BearerWiMAX:
			qDebug() << "QNetworkConfiguration::BearerWiMAX";
			setIsSlowNetwork(true);
		break;
		default:
			qDebug() << "QNetworkConfiguration::default...";
			setIsSlowNetwork(true);

		}
	}
}

void System::onNetworkConfigurationChanged(QNetworkConfiguration )
{
	onNetworkOnlineStateChanged( networkConfigurationManager.isOnline() );
}

void System::onNetworkConfigurationUpdateCompleted()
{
#ifdef Q_OS_IOS //iOS has not yet implemented QNetworkConfigurationManager
	onNetworkOnlineStateChanged( true );
#else
	onNetworkOnlineStateChanged( networkConfigurationManager.isOnline() );
#endif
}

const QString System::getDownloadPath() const
{
	QString downloadPath;
#if defined(Q_OS_ANDROID)
	//https://qt-project.org/forums/viewthread/35519
	QAndroidJniObject mediaDir = QAndroidJniObject::callStaticObjectMethod("android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;");
	QAndroidJniObject mediaPath = mediaDir.callObjectMethod( "getAbsolutePath", "()Ljava/lang/String;" );
	downloadPath = mediaPath.toString()+"/Download/";
	QAndroidJniEnvironment env;
	if (env->ExceptionCheck()) {
		// Handle exception here.
		qDebug() << "[ERR]:Android(Extras):getting SD card download path";
		env->ExceptionClear();
	}
#elif defined(Q_OS_IOS)
	QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
	qDebug() << "[IOS]download paths count:" << paths.size();
	downloadPath = paths.first();
#else
	QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
	qDebug() << "[Other OS]download paths count:" << paths.size();
	downloadPath = paths.first();

#endif
	qDebug() << "downloadPath:" << downloadPath;
	return downloadPath;
}

#ifndef Q_OS_IOS
int System::getIOSVersion()
{
	return -1;
}
#endif

void System::setSoftInputMode(SoftInputMode softInputMode)
{
#ifdef Q_OS_ANDROID
	jint SOFT_INPUT_ADJUST_NOTHING = QAndroidJniObject::getStaticField<jint>(
				"android/view/WindowManager$LayoutParams", "SOFT_INPUT_ADJUST_NOTHING");
	qDebug() << __FUNCTION__ << "SOFT_INPUT_ADJUST_NOTHING=" << SOFT_INPUT_ADJUST_NOTHING;

	jint SOFT_INPUT_ADJUST_UNSPECIFIED = QAndroidJniObject::getStaticField<jint>(
				"android/view/WindowManager$LayoutParams", "SOFT_INPUT_ADJUST_UNSPECIFIED");
	qDebug() << __FUNCTION__ << "SOFT_INPUT_ADJUST_UNSPECIFIED=" << SOFT_INPUT_ADJUST_UNSPECIFIED;

	jint SOFT_INPUT_ADJUST_RESIZE = QAndroidJniObject::getStaticField<jint>(
				"android/view/WindowManager$LayoutParams", "SOFT_INPUT_ADJUST_RESIZE");
	qDebug() << __FUNCTION__ << "SOFT_INPUT_ADJUST_RESIZE=" << SOFT_INPUT_ADJUST_RESIZE;

	jint SOFT_INPUT_ADJUST_PAN = QAndroidJniObject::getStaticField<jint>(
				"android/view/WindowManager$LayoutParams", "SOFT_INPUT_ADJUST_PAN");
	qDebug() << __FUNCTION__ << "SOFT_INPUT_ADJUST_PAN=" << SOFT_INPUT_ADJUST_PAN;

	jint param = -1;
	switch ( softInputMode )
	{
	case ADJUST_NOTHING:
		param = SOFT_INPUT_ADJUST_NOTHING;
	break;
	case ADJUST_UNSPECIFIED:
		param = SOFT_INPUT_ADJUST_UNSPECIFIED;
	break;
	case ADJUST_RESIZE:
		param = SOFT_INPUT_ADJUST_RESIZE;
	break;
	case ADJUST_PAN:
		param = SOFT_INPUT_ADJUST_PAN;
	break;
	default:
		param = -1;
	}
	if ( param != -1 )
	{
		QAndroidJniObject activity = QtAndroid::androidActivity();
		qDebug() << __FUNCTION__ << "activity.isValid()=" << activity.isValid();

		QAndroidJniObject window = activity.callObjectMethod(
					"getWindow","()Landroid/view/Window;");
		qDebug() << __FUNCTION__ << "window.isValid()=" << window.isValid();

		QAndroidJniEnvironment env;
		if (env->ExceptionCheck())
		{
			// Handle exception here.
			qDebug() << "Exception 1....";
			env->ExceptionDescribe();
			env->ExceptionClear();
		}


		if ( window.isValid() )
		{
			qDebug() << __FUNCTION__ << ":param=" << param;
			window.callMethod<void>("setSoftInputMode","(I)V",param);
		}
	}

#endif
}

#ifndef Q_OS_IOS
void System::enableAutoScreenOrientation(bool enable)
{
#ifdef Q_OS_ANDROID
	QAndroidJniObject activity = QtAndroid::androidActivity();
	qDebug() << __FUNCTION__ << "activity.isValid()=" << activity.isValid();

	QAndroidJniObject contentResolver = activity.callObjectMethod("getContentResolver","()Landroid/content/ContentResolver;");
	qDebug() << __FUNCTION__ << "contentResolver.isValid()=" << contentResolver.isValid();

	QAndroidJniObject Settings__System__ACCELEROMETER_ROTATION = QAndroidJniObject::getStaticObjectField(
				"android/provider/Settings$System", "ACCELEROMETER_ROTATION", "Ljava/lang/String;");
	qDebug() << __FUNCTION__ << "ACCELEROMETER_ROTATION.isValid()=" << Settings__System__ACCELEROMETER_ROTATION.isValid();

	jint orig = (bool)QAndroidJniObject::callStaticMethod<jint>(
				"android.provider.Settings$System", "getInt",
				"(Landroid/content/ContentResolver;Ljava/lang/String;)I",
				contentResolver.object<jobject>(),
				Settings__System__ACCELEROMETER_ROTATION.object<jstring>()
	);
	qDebug() << "orig=" << orig;


	//Settings.Secure.getInt(mContentResolver,mSettingName

	bool r = (bool)QAndroidJniObject::callStaticMethod<jboolean>(
				"android.provider.Settings$System", "putInt",
				"(Landroid/content/ContentResolver;Ljava/lang/String;I)B",
				contentResolver.object<jobject>(),
				Settings__System__ACCELEROMETER_ROTATION.object<jstring>(),
				enable?1:0
	);
	qDebug() << __FUNCTION__ << ":r=" << r;

	/*  http://stackoverflow.com/questions/3611457/android-temporarily-disable-orientation-changes-in-an-activity
None of the other answers did the trick perfectly for me, but here's what I found that does.

Lock orientation to current...

if(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT) {
	setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
} else setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

When changing orientation should be allowed again, set back to default...

setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);

	 */

#endif
	__IMPLEMENTATION_DETAIL_ENABLE_AUTO_ROTATE = enable;
}
#endif

void System::test()
{
#ifdef Q_OS_ANDROID
	/*myUsbManager = (UsbManager) getSystemService(USB_SERVICE);
			HashMap<String, UsbDevice> deviceList = myUsbManager.getDeviceList();

			if (!deviceList.isEmpty())
			{
				for (UsbDevice device : deviceList.values())
				{
					// 枚举设备
					if (device.getVendorId() == VendorID && device.getProductId() == ProductID)
					{
						myUsbDevice = device;
					}
				}
			}
	*/
	int VendorID = 0;
	int ProductID = 0;
	QAndroidJniObject myUsbDevice;

	QAndroidJniObject activity = QtAndroid::androidActivity();
	qDebug() << __FUNCTION__ << "activity.isValid()=" << activity.isValid();

	QAndroidJniObject Context__USB_SERVICE = QAndroidJniObject::getStaticObjectField(
				"android/content/Context", "USB_SERVICE", "Ljava/lang/String;");
	qDebug() << __FUNCTION__ << "Context__USB_SERVICE.isValid()=" << Context__USB_SERVICE.isValid();

	//myUsbManager = (UsbManager) getSystemService(USB_SERVICE);
	QAndroidJniObject myUsbManager = activity.callObjectMethod(
				"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;",
				Context__USB_SERVICE.object<jobject>() );
	qDebug() << __FUNCTION__ << "myUsbManager.isValid()=" << myUsbManager.isValid();

	//HashMap<String, UsbDevice> deviceList = myUsbManager.getDeviceList();
	QAndroidJniObject deviceList = myUsbManager.callObjectMethod(
				"getDeviceList","()Ljava/util/HashMap;");
	qDebug() << __FUNCTION__ << "deviceList.isValid()=" << deviceList.isValid();


	jboolean isEmpty = true;
	isEmpty = deviceList.callMethod<jboolean>("isEmpty");
	if ( !isEmpty )
	{
		//HashMap - Collection<V> 	values()
		QAndroidJniObject values = deviceList.callObjectMethod(
					"values","()Ljava/util/Collection;");
		qDebug() << __FUNCTION__ << "values.isValid()=" << values.isValid();

		//Collection - abstract Object[] 	toArray()
		QAndroidJniObject valuesArrayObj = values.callObjectMethod(
					"toArray","()[Ljava/lang/Object;");
		qDebug() << __FUNCTION__ << "valuesArrayObj.isValid()=" << valuesArrayObj.isValid();

		QAndroidJniEnvironment env;

		jobjectArray valuesArray = valuesArrayObj.object<jobjectArray>();
		const int valuesCount = env->GetArrayLength(valuesArray);
		qDebug() << __FUNCTION__ << ":valuesArray=" << (void*)valuesArray;
		qDebug() << __FUNCTION__ << ":valuesCount=" << valuesCount;

		for ( int i = 0 ; i < valuesCount ; i++ )
		{
			jobject value = env->GetObjectArrayElement(valuesArray, i);

			QAndroidJniObject device(value);

			//device.getVendorId()
			jint vendorId = deviceList.callMethod<jint>("getVendorId","()I");
			qDebug() << "i=" << i << ":vendorId=" << vendorId;
			//device.getProductId()
			jint productId = deviceList.callMethod<jint>("getProductId","()I");
			qDebug() << "i=" << i << ":productId=" << productId;
			if ( productId == ProductID && vendorId == VendorID )
			{
				myUsbDevice = device;
			}
		}

	}

#endif
}

void System::downloadURL(const QString& url)
{
#ifdef Q_OS_ANDROID
/*
	String serviceString = Context.DOWNLOAD_SERVICE;
	DownloadManager downloadManager;
	downloadManager = (DownloadManager)getSystemService(serviceString);

	Uri uri = Uri.parse("http://developer.android.com/shareables/icon_templates-v4.0.zip");
	DownloadManager.Request request = new Request(uri);
	long reference = downloadManager.enqueue(request);
 */

	QAndroidJniObject Context__DOWNLOAD_SERVICE = QAndroidJniObject::getStaticObjectField(
				"android/content/Context", "DOWNLOAD_SERVICE", "Ljava/lang/String;");
	qDebug() << __FUNCTION__ << "Context__DOWNLOAD_SERVICE.isValid()=" << Context__DOWNLOAD_SERVICE.isValid();

	QAndroidJniObject activity = QtAndroid::androidActivity();
	qDebug() << __FUNCTION__ << "activity.isValid()=" << activity.isValid();

	QAndroidJniObject downloadManager = activity.callObjectMethod(
		"getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
		Context__DOWNLOAD_SERVICE.object<jstring>()
	);
	qDebug() << __FUNCTION__ << "downloadManager.isValid()=" << downloadManager.isValid();

	QAndroidJniObject urlJavaString = QAndroidJniObject::fromString(url);

	QAndroidJniObject uri = QAndroidJniObject::callStaticObjectMethod(
				"android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
				urlJavaString.object<jstring>() );
	qDebug() << __FUNCTION__ << "uri.isValid()=" << uri.isValid();


	QAndroidJniObject request = QAndroidJniObject(
				"android/app/DownloadManager$Request","(Landroid/net/Uri;)V",
				uri.object<jobject>()
	);
	qDebug() << __FUNCTION__ << "request.isValid()=" << request.isValid();

	jlong reference = downloadManager.callMethod<jlong>(
				"enqueue","(Landroid/app/DownloadManager$Request;)J",
				request.object<jobject>());
	qDebug() << "reference=" << reference;

#endif
}
/*
QString System::saveQMLImage(QObject *qmlImage,
						  const QString& fileBaseName,
						  const QString& albumName,
						  QString albumParentPath )
{
	QGraphicsObject *item = qobject_cast<QGraphicsObject*>(qmlImage);

	if ( item == 0 )
	{
		qDebug() << "Item is NULL";
		return;
	}

	QImage img(item->boundingRect().size().toSize(), QImage::Format_RGB32);
	img.fill(QColor(255, 255, 255).rgb());
	QPainter painter(&img);
	QStyleOptionGraphicsItem styleOption;
	item->paint(&painter, &styleOption);

	if ( albumParentPath.isEmpty() )
	{
#if defined(Q_OS_ANDROID)
		//https://qt-project.org/forums/viewthread/35519
		QAndroidJniObject mediaDir = QAndroidJniObject::callStaticObjectMethod("android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;");
		QAndroidJniObject mediaPath = mediaDir.callObjectMethod( "getAbsolutePath", "()Ljava/lang/String;" );
		albumParentPath = mediaPath.toString();
		QAndroidJniEnvironment env;
		if (env->ExceptionCheck()) {
			// Handle exception here.
			qDebug() << "[ERR]:Android(Extras):getting SD card path";
			env->ExceptionClear();
		}
#elif defined(Q_OS_IOS)
		QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
		qDebug() << "[IOS]album parent paths count:" << paths.size();
		albumParentPath = paths.first();
#else
		QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
		qDebug() << "[Other OS]album parent paths count:" << paths.size();
		albumParentPath = paths.first();

#endif

	}

	QString path = albumParentPath.append("/").append(albumName).append("/").append(fileBaseName);
	img.save(path);
	return path;
}
*/

QString System::createAlbumPath(const QString& albumName)
{
	QString parentPath;

#if defined(Q_OS_ANDROID)
	QAndroidJniObject Environment__MEDIA_MOUNTED = QAndroidJniObject::getStaticObjectField(
				"android/os/Environment", "MEDIA_MOUNTED", "Ljava/lang/String;");
	qDebug() << "Environment__MEDIA_MOUNTED:isValid:" << Environment__MEDIA_MOUNTED.isValid() << ":toString:" << Environment__MEDIA_MOUNTED.toString();
	QString  Environment__MEDIA_MOUNTED_STRING =	Environment__MEDIA_MOUNTED.toString();
	QAndroidJniObject mediaState = QAndroidJniObject::callStaticObjectMethod("android/os/Environment", "getExternalStorageState", "()Ljava/lang/String;");
	QString mediaStateString = mediaState.toString();

	qDebug() <<"mediaStateString:"<< mediaStateString;

	if(mediaStateString != Environment__MEDIA_MOUNTED_STRING )
	{
		qDebug() << "No SD Card...";
		return QString();
	}

		//https://qt-project.org/forums/viewthread/35519
		QAndroidJniObject mediaDir = QAndroidJniObject::callStaticObjectMethod("android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;");
		QAndroidJniObject mediaPath = mediaDir.callObjectMethod( "getAbsolutePath", "()Ljava/lang/String;" );
		parentPath = mediaPath.toString();
		QAndroidJniEnvironment env;
		if (env->ExceptionCheck()) {
			// Handle exception here.
			qDebug() << "[ERR]:Android(Extras):getting SD card path";
			env->ExceptionClear();
		}
#elif defined(Q_OS_IOS)
		QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
		qDebug() << "[IOS]album parent paths count:" << paths.size();
		parentPath = paths.first();
#else
		QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
		qDebug() << "[Other OS]album parent paths count:" << paths.size();
		parentPath = paths.first();

#endif

		QDir dir(parentPath);

		qDebug() << __FUNCTION__ << ":dir=" << dir.dirName();

		QString userAlbumPath = parentPath.append("/").append(albumName);;
		QDir userAlbumDir = userAlbumPath;
		if( !userAlbumDir.exists() )
		{
			qDebug() << albumName << ":not exists, create it";
			dir.mkpath( albumName );
		}
		else
		{
			qDebug() << albumName << ":exists";
		}

		return userAlbumPath;
}

#ifndef Q_OS_IOS
void System::addToImageGallery(const QString& imageFullPath)
{

#if defined(Q_OS_ANDROID)
	/*
	 http://stackoverflow.com/questions/2170214/image-saved-to-sdcard-doesnt-appear-in-androids-gallery-app
	File imageFile = ...
	MediaScannerConnection.scanFile(this, new String[] { imageFile.getPath() }, new String[] { "image/jpeg" }, null);
	 */

	QAndroidJniObject activity = QtAndroid::androidActivity();
	qDebug() << __FUNCTION__ << "activity.isValid()=" << activity.isValid();

	QAndroidJniEnvironment env;

	//new String[] { imageFile.getPath() }
	jobjectArray imageFilePaths = (jobjectArray)env->NewObjectArray(
		1,
		env->FindClass("java/lang/String"),
		(jobject)0
	);
	QAndroidJniObject imagePathJString = QAndroidJniObject::fromString(imageFullPath);
	env->SetObjectArrayElement(
		imageFilePaths, 0, imagePathJString.object<jstring>() );

	//new String[] { "image/jpeg", "image/png", "image/gif" }
	jobjectArray imageFileTypes = (jobjectArray)env->NewObjectArray(
		3,
		env->FindClass("java/lang/String"),
		(jobject)0
	);
	QAndroidJniObject imageType1 = QAndroidJniObject::fromString("image/jpeg");
	env->SetObjectArrayElement(
		imageFileTypes, 0, imageType1.object<jstring>() );
	QAndroidJniObject imageType2 = QAndroidJniObject::fromString("image/png");
	env->SetObjectArrayElement(
		imageFileTypes, 1, imageType2.object<jstring>() );
	QAndroidJniObject imageType3 = QAndroidJniObject::fromString("image/gif");
	env->SetObjectArrayElement(
		imageFileTypes, 2, imageType3.object<jstring>() );


	QAndroidJniObject::callStaticMethod<void>("android/media/MediaScannerConnection",
											  "scanFile",
											  "(Landroid/content/Context;[Ljava/lang/String;[Ljava/lang/String;Landroid/media/MediaScannerConnection$OnScanCompletedListener;)V",
											  activity.object<jobject>(),
											  imageFilePaths, imageFileTypes, (jobject)0);



	env->DeleteLocalRef(imageFilePaths);
	env->DeleteLocalRef(imageFileTypes);

#else

#endif


}
#endif


#ifndef Q_OS_IOS
void System::registerApplePushNotificationService()
{
}
#endif


QTimeZone System::createTimeZone(const QString& ianaId)
{
	//to handle bug before Qt 5.5.0: https://bugreports.qt.io/browse/QTBUG-35908
#ifdef Q_OS_ANDROID
	if ( ianaId == "Asia/Hong_Kong"
		 || ianaId == "Asia/Shanghai"
	)
		return QTimeZone("UTC+08:00");
	else if ( ianaId == "America/Los_Angeles" )
		return QTimeZone("UTC-07:00");//actually can be UTC-7 (DST) or UTC-8

	else
		return QTimeZone("UTC");
#else
	return QTimeZone(ianaId.toLatin1());
#endif
}

QByteArray System::getSystemTimezoneId()
{
	//to handle bug before Qt 5.5.0: https://bugreports.qt.io/browse/QTBUG-35908
#ifdef Q_OS_ANDROID
	QAndroidJniObject sysTZ = QAndroidJniObject::callStaticObjectMethod(
			"java/util/TimeZone", "getDefault", "()Ljava/util/TimeZone;");
	qDebug() << "sysTZ.isValid()=" << sysTZ.isValid();
	QAndroidJniObject sysTZID = sysTZ.callObjectMethod("getID","()Ljava/lang/String;");
	qDebug() << "sysTZID.isValid()=" << sysTZID.isValid();
	return sysTZID.toString().toLatin1();
#else
	return QTimeZone::systemTimeZoneId();
#endif
}

}//namespace qt
}//namespace wpp
