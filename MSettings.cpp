#include "MSettings.h"
//#include "MLogger.h"


QVariant MSettings::value( const QString & key, const QVariant & defaultValue ) {
	//MLogger::info("MSettings::value _ "+key);

	QVariant variant = instance()->QSettings::value(key);
	if(!variant.isValid()) {
		instance()->QSettings::setValue(key,defaultValue);
		variant = defaultValue;
	};
	
	return variant;
};

void MSettings::setValue( const QString & key, const QVariant & value ) {
	//MLogger::info("into  MSettings::setValue");
	instance()->QSettings::setValue(key,value);
};

MSettings * MSettings::ptr = 0;
MSettings * MSettings::instance() {
	if(!ptr)
		ptr = new MSettings();
	return ptr;
};

MSettings::MSettings()
    :QSettings("config.ini",QSettings::IniFormat,0)
{
	/* MLogger::info("into  MSettings::MSettings()");
	MLogger::info("file - "+fileName());
	MLogger::info("fallbacksEnabled - "+QString::number(fallbacksEnabled())); */

};
