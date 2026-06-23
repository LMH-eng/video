配置文件ini
void CKernel::loadIniFile()

{

    //默认值

    m_ip = "192.168.5.198";

    m_port = "8004";

  

    //获取exe 目录  C:/build-debug

    QString path = QCoreApplication::applicationDirPath() + "/config.ini";

    //根据目录

    //看文件是否存在 存在加载 不存在创建并且写入默认值

    QFileInfo info( path );

    if( info.exists() ){

        //存在

        QSettings setting( path , QSettings::IniFormat );

        //打开组

        setting.beginGroup( "net" );

        QVariant strIP = setting.value( "ip" , "" );

        QVariant strPort = setting.value( "port" , "" );

        if( !strIP.toString().isEmpty() ) m_ip = strIP.toString();

        if( !strPort.toString().isEmpty() ) m_port = strPort.toString();

        //关闭组

        setting.endGroup();

    }else{

        //不存在

        QSettings setting( path , QSettings::IniFormat ); //没有会创建

        //打开组

        setting.beginGroup( "net" );

        //设置 key value

        setting.setValue( "ip" , m_ip );

        setting.setValue( "port" , m_port );

        //关闭组

        setting.endGroup();

    }

    qDebug() << "ip:"<<m_ip <<" port:"<< m_port;

}