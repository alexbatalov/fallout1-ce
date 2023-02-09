package com.alexbatalov.falloutce;

import android.content.ContentResolver;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class FileUtils {

    static boolean copyRecursively(ContentResolver contentResolver, DocumentFile src, File dest) {
        final DocumentFile[] documentFiles = src.listFiles();
        for (final DocumentFile documentFile : documentFiles) {
            if (documentFile.isFile()) {
                if (!copyFile(contentResolver, documentFile, new File(dest, documentFile.getName()))) {
                    return false;
                }
            } else if (documentFile.isDirectory()) {
                final File subdirectory = new File(dest, documentFile.getName());
                if (!subdirectory.exists()) {
                    subdirectory.mkdir();
                }

                if (!copyRecursively(contentResolver, documentFile, subdirectory)) {
                    return false;
                }
            }
        }
        return true;
    }

    private static boolean copyFile(ContentResolver contentResolver, DocumentFile src, File dest) {
        try {
            final InputStream inputStream = contentResolver.openInputStream(src.getUri());
            final OutputStream outputStream = new FileOutputStream(dest);

            final byte[] buffer = new byte[16384];
            int bytesRead;
            while ((bytesRead = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, bytesRead);
            }

            inputStream.close();
            outputStream.close();
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }

        return true;
    }
}
