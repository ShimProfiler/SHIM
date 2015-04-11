/*
 *  This file is part of the Jikes RVM project (http://jikesrvm.org).
 *
 *  This file is licensed to You under the Eclipse Public License (EPL);
 *  You may not use this file except in compliance with the License. You
 *  may obtain a copy of the License at
 *
 *      http://www.opensource.org/licenses/eclipse-1.0.php
 *
 *  See the COPYRIGHT.txt file distributed with this work for information
 *  regarding copyright ownership.
 */
package moma;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.jikesrvm.VM;
import org.vmmagic.unboxed.Address;
import org.vmmagic.unboxed.Extent;
import org.vmmagic.unboxed.Offset;
import org.vmmagic.unboxed.Word;

/**
 * Static utility methods related to the user interface.
 */
public final class Util {

  private static final String[] BITS = {"0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
                                        "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"};

  public static String dehtmlize(final String html) {
    return html.replaceAll("<[^>]*>", "").replaceAll("&lt;", "<").replaceAll("&gt;", ">").replaceAll("&amp;", "&");
  }

  public static String escapeHtml(final String s) {
    // the order is important, otherwise '<' is replaced with '&lt;' and
    // then later '&' is replaced with '&amp;' so '<' becomes '&amp;lt;'
    return s.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;");
  }

  /**
   * Given a type name like "I" or "[C" or "Ljava/lang/String;" or
   * "[[Ljava/lang/String;", return a shorter version without package names.
   */
  public static String shortenTypeName(final String name) {
    if (name.contains("/")) {
      return name.replaceFirst("L.*/", "").replace(";", "");
    } else {
      return name;
    }
  }

  /**
   * Given a type name like "[C" or "Ljava/lang/String;" or
   * "[[Ljava/lang/String;", return a human readable version
   * like "char[]" or "java.lang.String" or "java.lang.String[][]".
   */
  public static String humanizeTypeName(final String name) {
    return humanizeTypeName(name, false, false);
  }

  public static String humanizeTypeName(final String name, final boolean htmlize, final boolean abbreviate) {
    String h = name;
    while (h.startsWith("[")) {
      h = h.replaceFirst("\\[", "") + "[]";
    }
    if (h.startsWith("L")) {
      h = h.replaceFirst("L", "").replace(";", "");
      if (abbreviate) {
        h = h.replaceFirst("org/jikesrvm", "~"); // abbreviate org.jikesrvm as ~
      }
      final int startOfUnqualifiedName = h.contains("/") ? h.lastIndexOf('/') + 1 : 0;
      h = h.replace("/", ".");
      if (htmlize) {
        h = h.substring(0, startOfUnqualifiedName)+"<b>"+h.substring(startOfUnqualifiedName)+"</b>";
      }
      return h;
    }
    if (h.matches("B(\\[\\])*")) {
      h = h.replaceFirst("B", "byte");
    } else if (h.matches("Z(\\[\\])*")) {
      h = h.replaceFirst("Z", "boolean");
    } else if (h.matches("C(\\[\\])*")) {
      h = h.replaceFirst("C", "char");
    } else if (h.matches("S(\\[\\])*")) {
      h = h.replaceFirst("S", "short");
    } else if (h.matches("I(\\[\\])*")) {
      h = h.replaceFirst("I", "int");
    } else if (h.matches("J(\\[\\])*")) {
      h = h.replaceFirst("J", "long");
    } else if (h.matches("F(\\[\\])*")) {
      h = h.replaceFirst("F", "float");
    } else if (h.matches("D(\\[\\])*")) {
      h = h.replaceFirst("D", "double");
    } else if (h.matches("V")) {
      h = h.replaceFirst("V", "void");
    }
    if (htmlize) {
      return "<b>"+h+"</b>";
    } else {
      return h;
    }
  }

  /**
   * Given a method descriptor like "()Z" or "(II)Ljava/lang/String;"
   * return a human readable version
   * like "():boolean" or "(int, int):java.lang.String".
   */
  public static String humanizeMethodDescriptor(final String name) {
    final Pattern p = Pattern.compile("\\((.*)\\)(.*)");
    final Matcher m = p.matcher(name);
    final boolean b = m.matches();
    if (b) {
      return "(" + parseArguments(m.group(1)) + "): " + humanizeTypeName(m.group(2), false, true);
    }
    return name;
  }

  public static String humanizeMemberDescriptor(final String descriptor) {
    if (descriptor.startsWith("(")) {
      return Util.humanizeMethodDescriptor(descriptor);
    }
    return Util.humanizeTypeName(descriptor, false, true);
  }

  private static String parseArguments(final String args) {
    final Pattern p = Pattern.compile("(\\[*(B|Z|C|S|I|J|F|D|(L[^;]+;)))(.*)");
    final Matcher m = p.matcher(args);
    final boolean b = m.matches();
    if (b) {
      final String x = humanizeTypeName(m.group(1), false, true);
      final String xs = parseArguments(m.group(4));
      if (xs.equals("")) return x;
      return x + ", " + xs;
    }
    return args;
  }

  public static String shorten(final String contents, final int maxLength) {
    if (contents.length() > maxLength) {
      return contents.substring(0, maxLength - 3) + "...";
    } else {
      return contents;
    }
  }

  public static String addressToHexString(final Address address) {
    if (address == null)   return "?";
    System.out.println("ToHexString " + address.toInt());
    if (VM.BuildFor32Addr) return String.format("%8x", address.toInt());
                           return String.format("%16x", address.toLong());
  }

  public static Address fromHex(final String addressAsHexString) {
    if (VM.BuildFor32Addr) {
      // parseInt does not work for negative integers like BFFFF3B0
      // because it expects negative integers to be like -40000C50
      // (this is 2's complement of BFFFF3B0 and a sign)
      // FIXME find something better or error on invalid hex strings
      int value = 0;
      for (final char c : addressAsHexString.toUpperCase().toCharArray()) {
        value = ((value << 4) & 0xfffffff0) | (0xf & (('0' <= c && c <= '9') ? c - '0' : c - '7'));
      }
      return Address.fromIntZeroExtend(value);
      // return Address.fromIntZeroExtend(Integer.parseInt(addressAsHexString, 16));
    }
    long value = 0;
    for (final char c : addressAsHexString.toUpperCase().toCharArray()) {
      value = ((value << 4) & 0xfffffffffffffff0L) | (0xfL & (('0' <= c && c <= '9') ? c - '0' : c - '7'));
    }
    return Address.fromLong(value);
    // return Address.fromLong(Long.parseLong(addressAsHexString, 16));
  }

  public static String wordToHexString(final Word word) {
    if (word == null) {
      return "?";
    } else if (VM.BuildFor32Addr) {
      final int intValue = word.toInt();
      return String.format("%08x", intValue);
    } else {
      final long longValue = word.toLong();
      return String.format("%016x", longValue);
    }
  }

  public static String offsetToDecString(final Offset offset) {
    if (offset == null) {
      return "?";
    } else if (VM.BuildFor32Addr) {
      final int intValue = offset.toInt();
      return "" + intValue;
    } else {
      final long longValue = offset.toLong();
      return "" + longValue;
    }
  }

  public static String offsetToHexString(final Offset offset) {
    if (offset == null) {
      return "?";
    } else if (VM.BuildFor32Addr) {
      final int intValue = offset.toInt();
      return String.format("%08x", intValue);
    } else {
      final long longValue = offset.toLong();
      return String.format("%016x", longValue);
    }
  }

  public static String offsetToPlusMinusHexString(final Offset offset) {
    if (offset == null) {
      return "?";
    } else if (VM.BuildFor32Addr) {
      final int intValue = offset.toInt();
      return intValue<0?String.format("-%x", -intValue):String.format("+%x", intValue);
    } else {
      final long longValue = offset.toLong();
      return longValue<0?String.format("-%x", -longValue):String.format("+%x", longValue);
    }
  }

  public static String offsetToNothingMinusHexString(final Offset offset) {
    if (offset == null) {
      return "?";
    } else if (VM.BuildFor32Addr) {
      final int intValue = offset.toInt();
      return intValue<0?String.format("-%x", -intValue):String.format("%x", intValue);
    } else {
      final long longValue = offset.toLong();
      return longValue<0?String.format("-%x", -longValue):String.format("%x", longValue);
    }
  }

  public static String extentToDecString(final Extent extent) {
    if (extent == null) {
      return "?";
    } else if (VM.BuildFor32Addr) {
      final int intValue = extent.toInt();
      return "" + intValue;
    } else {
      final long longValue = extent.toLong();
      return "" + longValue;
    }
  }

  public static String extentToHexString(final Extent extent) {
    if (extent == null) {
      return "?";
    } else if (VM.BuildFor32Addr) {
      final int intValue = extent.toInt();
      return String.format("%08x", intValue);
    } else {
      final long longValue = extent.toLong();
      return String.format("%016x", longValue);
    }
  }

  /**
   *
   * @param value
   * @param sizeInBits
   * @param blockSize number of bits in a block, blocks separated by spaces,
   *        e.g 0111 0010 with blockSize equal 4, can't be less than 4.
   * @return string of 0s and 1s representing value as a binary number
   */
  public static String toBinaryString(final long value, final int sizeInBits, final int blockSize) {
    final StringBuffer sb = new StringBuffer();
    final int size = ceilingPower2(sizeInBits);
    final int block = ceilingPower2(blockSize);
    int remainder = block;
    for (int i = size - 4; i >= 0; i -= 4) {
      if (block > 0 && remainder == 0) {
        remainder = block;
        sb.append(' ');
      }
      final int index = (0xf & (int)(value >> i));
      sb.append(BITS[index]);
      remainder -= 4;
    }
    return sb.toString();
  }

  public static int ceilingPower2(final int x) {
    int r = x - 1;
    r = r | (r >>  1);
    r = r | (r >>  2);
    r = r | (r >>  4);
    r = r | (r >>  8);
    r = r | (r >> 16);
    return r + 1;
  }
}
