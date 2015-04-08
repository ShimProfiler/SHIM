package moma;

import org.vmmagic.pragma.NonMoving;
import org.vmmagic.unboxed.Address;

/**
 * Created by xiyang on 15/09/2014.
 */
@NonMoving
public class MoMaProfiler extends Thread{

  public Address nativeFlag;

  private static native int initEvents(int nrEvents);
  private static native void createEvent(int id, String eventName);
  private static native void test();
  private static native void sampling(Address tagAddress);

  public void init(){
    System.out.println("MomaProfiler:init is called");
  }

  public void beginProfile(){

  }

  public void profile() {
    try {
      Thread.sleep(10000);
    }catch(Exception e){
      ;
    }
  }

  public void process() {

  }

  public void endProfile(){

  }

  public void run() {
    init();
    synchronized (this) {
      try {
        this.wait();
      } catch (Exception e) {
        System.out.println(e);
      }
    }
    beginProfile();
    while (true) {
      profile();
      process();
      if (enabled == false) {
        endProfile();
        break;
      }
    }
    enabled = true;
  }

}
