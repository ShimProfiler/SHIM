package moma;

import java.io.*;
import java.util.ArrayList;

import org.jikesrvm.ArchitectureSpecific;
import org.jikesrvm.adaptive.util.CompilerAdviceAttribute;
import org.jikesrvm.adaptive.util.CompilerAdviceInfoReader;
import org.jikesrvm.classloader.RVMClass;
import org.jikesrvm.classloader.RVMClassLoader;
import org.jikesrvm.classloader.RVMMethod;
import org.jikesrvm.classloader.TypeReference;
import org.vmmagic.unboxed.Address;
import org.vmmagic.unboxed.ObjectReference;

/**
 * Created by xiyang on 16/10/2014.
 * Disassemble codearry to instructions, and print them out
 */
public class Disassemble implements MomaCallBack{

  public void invoke(String target){
    System.out.println("Disassemble method:" + target);
    String[] code = disassemble(target);
    for(String l : code){
      System.out.println(l);
    }
  }

  public static String[] disassemble(String target){

    CompilerAdviceAttribute attr = CompilerAdviceInfoReader.readOneAttributeFromString(target);

    if (attr == null)
      return null;

    // find class loader from get
    ClassLoader cl = RVMClassLoader.findWorkableClassloader(attr.getClassName());
    if (cl == null){
      System.out.println("Disassembler could not find classloader of class " + attr.getClassName());
      return null;
    }

    TypeReference tRef = TypeReference.findOrCreate(cl, attr.getClassName());

    if (tRef == null){
      System.out.println("Disassembler could not find class " + attr.getClassName());
      return null;
    }

    RVMClass cls = (RVMClass) tRef.peekType();
    RVMMethod method = cls.findDeclaredMethod(attr.getMethodName(), attr.getMethodSig());
    if (method == null){
      System.out.println("Disassembler could not find method " + attr.getMethodSig() + " " + attr.getMethodName() +  " in class " + attr.getClassName());
      return null;
    }

    ArchitectureSpecific.CodeArray  ca =   method.getCurrentCompiledMethod().getEntryCodeArray();
    Address address = ObjectReference.fromObject(ca).toAddress();
    int length = ca.length();
    final Address end = address.plus(length);

    System.err.println("Disassemble code in address range [" + Util.addressToHexString(address) + ", " +
            Util.addressToHexString(end) + "[ (" + length + " bytes)");
    OutputStream os = null;
    try {
      // create a temporary file containing the code
      final File file = File.createTempFile("jikesrdb-binary-for-disassembly", ".bin");
      System.err.println("  Generating binary file with code: " + file);
      os = new BufferedOutputStream(new FileOutputStream(file));
      Address current = address;
      while (current.LT(end)) {
        os.write(current.loadByte());
        current = current.plus(1);
      }
      os.close();
      // launch ndisasm
      System.err.print("  Lanuching ndisasm to disassemble the generated file \"");
      final String[] commandLineWords = new String[]{"/usr/bin/ndisasm", "-b", "32", "-o", "0x" + Util.addressToHexString(address),
              file.getPath()};

      final StringBuffer commandLine = new StringBuffer();
      for (final String word : commandLineWords) {
        commandLine.append(word);
        commandLine.append(' ');
        System.err.print(" " + word);

      }
      System.err.println("\"");
      // final Process ndisasm = Runtime.getRuntime().exec(commandLine);
      final ProcessBuilder builder = new ProcessBuilder(commandLineWords);
      builder.redirectErrorStream(true);
      final Process ndisasm = builder.start();
      // parse ndisasm output or error
      System.err.println("  Draining ndisasm's stdout/stderr");
      final ArrayList<String> stringList = new ArrayList<String>();
      final BufferedReader br = new BufferedReader(new InputStreamReader(ndisasm.getInputStream()));
      String line;
      while ((line = br.readLine()) != null) {
        stringList.add(line);
        System.out.println(line);
      }
      // delete temporary file
      //file.delete();
      // wait for ndisasm (should not block, given that we wait above for its
      // stdout to be drained)
      final int exitCode = ndisasm.waitFor();
      if (exitCode != 0) {
        stringList.add(0, "ERROR");
        stringList.add(1, commandLine.toString());
        stringList.add(2, "Exit code: " + exitCode);
      }
      // return
      return stringList.toArray(new String[stringList.size()]);
    } catch (final IOException ex) {
      ex.printStackTrace();
      return new String[]{ex.getMessage()};
    } catch (final InterruptedException ex) {
      ex.printStackTrace();
      return null;
    } finally {
      if (os != null) {
        try {
          os.close();
        } catch (final IOException ex) {
          // ignore
        }
      }
    }
  }
}
