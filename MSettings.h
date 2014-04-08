#ifndef MSETTINGS_H
#define MSETTINGS_H

#include <QSettings>

class MSettings : public QSettings
{
	Q_OBJECT
	
	public :
		static MSettings * instance();
		
		static QVariant value( const QString & key, const QVariant & defaultValue = QVariant() );
		static void setValue( const QString & key, const QVariant & value );

	private :
		static MSettings * ptr;
		MSettings();

};

#endif
