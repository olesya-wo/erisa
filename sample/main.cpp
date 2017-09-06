#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QDir>
#include <conio.h>
#include "xerisa.h"

void writeWav(QString filename, short bps, short channels, int freq, const void *data, int size)
{
    FILE *file;
    fopen_s(&file,filename.toLocal8Bit().constData(),"wb");

    fprintf_s(file, "RIFF");                             // +

    int chunkSize=size+(44-8);
    fwrite(&chunkSize, sizeof(chunkSize), 1, file);      // +

    fprintf_s(file, "WAVEfmt ");                         // +

    chunkSize=16;
    fwrite(&chunkSize, sizeof(chunkSize), 1, file);      // +

    short format=1;
    fwrite(&format, sizeof(format), 1, file);            // +

    fwrite(&channels, sizeof(channels), 1, file);        // +

    fwrite(&freq, sizeof(freq), 1, file);                // +

    int byteRate=freq * channels * bps / 8;
    fwrite(&byteRate, sizeof(byteRate), 1, file);        // +

    format=bps / 8 * channels;
    fwrite(&format, sizeof(format), 1, file);            // +

    fwrite(&bps, sizeof(bps), 1, file);                  // +

    fprintf_s(file, "data");                             // +

    fwrite(&size, sizeof(size), 1, file);                // +

    fwrite(data, 1, size, file);

    fclose(file);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    char c=0;
    eriInitializeLibrary();
    printf("Erisa ready.\n");
    QDir dir;
    if (a.arguments().length()==1){
        dir.setPath(dir.currentPath());
        printf("Directory was not specified, using current directory:\n");
        printf("%s\n\n",dir.path().toLocal8Bit().constData());
    }else if (a.arguments().length()==2){
        dir.setPath(a.arguments().at(1));
        if (dir.exists()){
            printf("Directory: %s\n\n",dir.path().toLocal8Bit().constData());
        }else{
            printf("Directory \"%s\" not found!\n",a.arguments().at(1).toLocal8Bit().constData());
            printf("Press any key to exit...\n");
            c = getch();
            return 0;
        }
    }else{
        printf("Please set one directory!\n");
        printf("Press any key to exit...\n");
        c = getch();
        return 0;
    }

    int count=0;
    QStringList list=dir.entryList(QStringList()<<"*.mio", QDir::Files | QDir::NoDotAndDotDot);
    if (list.isEmpty())
        printf("MIO files not found.\n");
    else
        printf("Processing MIO files...\n");
    foreach (QString s, list)
    {
        ++count;
        // Получить данные из файла и загрузить их в EMemoryFile
        QFile inp(dir.path()+"/"+s);
        if (!inp.open(QIODevice::ReadOnly))
        {
            printf("Can't open file %s!\n", s.toLocal8Bit().constData());
            continue;
        }
        QByteArray data=inp.readAll();
        inp.close();
        if (data.length()>500000000){
            printf("Too big file %s!\n",s.toLocal8Bit().constData());
            continue;
        }
        EMemoryFile	rf ;
        if ( rf.Open(data.constData(),data.length()) )
        {
            printf("Can't open memory from file %s!\n",s.toLocal8Bit().constData());
            continue;
        }

        // Считать из EMemoryFile классом MIODynamicPlayer
        MIODynamicPlayer mio;
        ESLError res=mio.Open(&rf);
        if (res!=eslErrSuccess)
        {
            printf("Invalid MIO-file %s!\n", s.toLocal8Bit().constData());
            continue;
        }
        printf("%i/%i\t",count,list.length());
        printf("%s: s",s.toLocal8Bit().constData());

        // Вывести данные о файле
        printf("BPS: %i ",mio.GetBitsPerSample());
        printf("Ch: %i ",mio.GetChannelCount());
        printf("Freq: %i ",mio.GetFrequency());
        printf("Samples: %i ",mio.GetTotalSampleCount());
        printf("Length: %is ",mio.GetTotalSampleCount()/mio.GetFrequency());

        QByteArray all_s;
        DWORD bytes=0;
        void *buff=0;
        do{
            buff=mio.GetNextWaveBuffer(bytes);
            if (buff)
            {
                all_s.append((const char*)buff,bytes);
                mio.DeleteWaveBuffer(buff);
            }else{
                printf("GetNextWaveBuffer return null! ");
                mio.Close();
                break;
            }
        }while(all_s.count()<mio.GetTotalSampleCount()*mio.GetChannelCount()*mio.GetBitsPerSample()/8);


        printf("Total: %i bytes\n",all_s.count());

        if (all_s.count()>0)
        {
            writeWav(dir.path()+"/"+s.left(s.length()-3)+"wav",mio.GetBitsPerSample(),mio.GetChannelCount(),mio.GetFrequency(),all_s.constData(),all_s.count());
        }
        mio.Close();
    }


    count=0;
    list=dir.entryList(QStringList()<<"*.eri", QDir::Files | QDir::NoDotAndDotDot);
    if (list.isEmpty())
        printf("ERI files not found.\n");
    else
        printf("Processing ERI files...\n");
    foreach (QString s, list)
    {
        ++count;
        // Получить данные из файла и загрузить их в EMemoryFile
        QFile inp(dir.path()+"/"+s);
        if (!inp.open(QIODevice::ReadOnly))
        {
            printf("Can't open file %s!\n", s.toLocal8Bit().constData());
            continue;
        }
        QByteArray data=inp.readAll();
        inp.close();
        if (data.length()>500000000){
            printf("Too big file %s!\n",s.toLocal8Bit().constData());
            continue;
        }
        EMemoryFile	rf ;
        if ( rf.Open(data.constData(),data.length()) )
        {
            printf("Can't open memory from file %s!\n",s.toLocal8Bit().constData());
            continue;
        }

        // Считать из EMemoryFile классом ERIAnimation
        ERIAnimation anim;
        ESLError res=anim.Open(&rf,ERISADecoder::dfTopDown);
        if (res!=eslErrSuccess)
        {
            printf("Invalid ERI-file %s!\n", s.toLocal8Bit().constData());
            continue;
        }
        printf("%i/%i\t",count,list.length());
        printf("%s: s",s.toLocal8Bit().constData());

        // Вывести данные о файле
        printf("Frames: %i ",anim.GetAllFrameCount());
        const EGL_IMAGE_INFO *eii=anim.GetImageInfo();
        printf("W: %i H: %i ",eii->dwImageWidth,eii->dwImageHeight);
        printf("BPP: %i ",eii->dwBitsPerPixel);

        // Сконвертировать в png
        switch (eii->fdwFormatType)
        {
        case EIF_GRAY_BITMAP:
        {
            printf("Format: Gray\n");
            QImage out((const uchar*)eii->ptrImageArray,eii->dwImageWidth,eii->dwImageHeight,QImage::Format_Grayscale8);
            // Сохранить
            out.save(dir.path()+"/"+s.left(s.length()-3)+"png");
            break;
        }
        case EIF_RGB_BITMAP:
        {
            printf("Format: RGB\n");
            QImage out((const uchar*)eii->ptrImageArray,eii->dwImageWidth,eii->dwImageHeight,QImage::Format_RGBA8888_Premultiplied);
            // Обменять R и B каналы местами
            QRgb cl;
            int r,g,b;
            for (int y=0; y<out.height();++y)
                for (int x=0; x<out.width();++x)
                {
                    cl=out.pixel(x,y);
                    r=qRed(cl); g=qGreen(cl); b=qBlue(cl);
                    out.setPixel(x,y,qRgba(b,g,r,255));
                }
            // Сохранить
            out=out.convertToFormat(QImage::Format_RGB888);
            out.save(dir.path()+"/"+s.left(s.length()-3)+"png");
            break;
        }
        case EIF_RGBA_BITMAP:
        {
            printf("Format: RGBA\n");
            QImage out((const uchar*)eii->ptrImageArray,eii->dwImageWidth,eii->dwImageHeight,QImage::Format_RGBA8888_Premultiplied);
            // Обменять R и B каналы местами
            QRgb cl;
            int r,g,b,al;
            for (int y=0; y<out.height();++y)
                for (int x=0; x<out.width();++x)
                {
                    cl=out.pixel(x,y);
                    r=qRed(cl); g=qGreen(cl); b=qBlue(cl); al=qAlpha(cl); al=qAlpha(cl);
                    out.setPixel(x,y,qRgba(b,g,r,al));
                }
            // Сохранить
            out.save(dir.path()+"/"+s.left(s.length()-3)+"png");
            break;
        }
        default:
            printf("Invalid FormatType - %i!\n",eii->fdwFormatType);
            continue;
        }
        if (anim.GetAllFrameCount()>1) // Это анимация
        {
            int frame=1;
            while (anim.SeekToNextFrame()==eslErrSuccess && frame<anim.GetAllFrameCount())
            {
                switch (eii->fdwFormatType)
                {
                case EIF_GRAY_BITMAP:
                {
                    QImage out((const uchar*)eii->ptrImageArray,eii->dwImageWidth,eii->dwImageHeight,QImage::Format_Grayscale8);
                    // Сохранить
                    out.save(dir.path()+"/"+s.left(s.length()-4)+"_"+QString::number(frame)+".png");
                    break;
                }
                case EIF_RGB_BITMAP:
                {
                    QImage out((const uchar*)eii->ptrImageArray,eii->dwImageWidth,eii->dwImageHeight,QImage::Format_RGBA8888_Premultiplied);
                    // Обменять R и B каналы местами
                    QRgb cl;
                    int r,g,b;
                    for (int y=0; y<out.height();++y)
                        for (int x=0; x<out.width();++x)
                        {
                            cl=out.pixel(x,y);
                            r=qRed(cl); g=qGreen(cl); b=qBlue(cl);
                            out.setPixel(x,y,qRgba(b,g,r,255));
                        }
                    // Сохранить
                    out=out.convertToFormat(QImage::Format_RGB888);
                    out.save(dir.path()+"/"+s.left(s.length()-4)+"_"+QString::number(frame)+".png");
                    break;
                }
                case EIF_RGBA_BITMAP:
                {
                    QImage out((const uchar*)eii->ptrImageArray,eii->dwImageWidth,eii->dwImageHeight,QImage::Format_RGBA8888_Premultiplied);
                    // Обменять R и B каналы местами
                    QRgb cl;
                    int r,g,b,al;
                    for (int y=0; y<out.height();++y)
                        for (int x=0; x<out.width();++x)
                        {
                            cl=out.pixel(x,y);
                            r=qRed(cl); g=qGreen(cl); b=qBlue(cl); al=qAlpha(cl); al=qAlpha(cl);
                            out.setPixel(x,y,qRgba(b,g,r,al));
                        }
                    // Сохранить
                    out.save(dir.path()+"/"+s.left(s.length()-4)+"_"+QString::number(frame)+".png");
                    break;
                }
                }
                ++frame;
            }
        }
        anim.Close();
    }
    printf("\nPress any key to exit...\n");
    c = getch();
    return 0;
}
