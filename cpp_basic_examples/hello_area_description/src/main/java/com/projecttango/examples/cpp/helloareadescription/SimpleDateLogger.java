package com.projecttango.examples.cpp.helloareadescription;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;

public class SimpleDateLogger {
    public static void logLocalTime(String metadataFile) {
        try {
            BufferedWriter dataWriter = new BufferedWriter(
                    new FileWriter(metadataFile, false));
            Date date = new Date();
            SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy_MM_dd_HH_mm_ss");
            String strDate = dateFormat.format(date);
            dataWriter.write("start_time: " + strDate + "\n");
            dataWriter.close();
        } catch (IOException err) {
            System.err.println(
                    "IOException in closing local time writer: " + err.getMessage());
        }
    }
}
