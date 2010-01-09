/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 */
package nemo;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;
import java.util.Collections;
import java.util.BitSet;
import org.apache.log4j.Logger;

import org.apache.thrift.*;
import org.apache.thrift.meta_data.*;
import org.apache.thrift.protocol.*;

public class ConstructionError extends Exception implements TBase, java.io.Serializable, Cloneable, Comparable<ConstructionError> {
  private static final TStruct STRUCT_DESC = new TStruct("ConstructionError");
  private static final TField MSG_FIELD_DESC = new TField("msg", TType.STRING, (short)1);

  public String msg;
  public static final int MSG = 1;

  // isset id assignments

  public static final Map<Integer, FieldMetaData> metaDataMap = Collections.unmodifiableMap(new HashMap<Integer, FieldMetaData>() {{
    put(MSG, new FieldMetaData("msg", TFieldRequirementType.DEFAULT, 
        new FieldValueMetaData(TType.STRING)));
  }});

  static {
    FieldMetaData.addStructMetaDataMap(ConstructionError.class, metaDataMap);
  }

  public ConstructionError() {
  }

  public ConstructionError(
    String msg)
  {
    this();
    this.msg = msg;
  }

  /**
   * Performs a deep copy on <i>other</i>.
   */
  public ConstructionError(ConstructionError other) {
    if (other.isSetMsg()) {
      this.msg = other.msg;
    }
  }

  @Override
  public ConstructionError clone() {
    return new ConstructionError(this);
  }

  public String getMsg() {
    return this.msg;
  }

  public ConstructionError setMsg(String msg) {
    this.msg = msg;
    return this;
  }

  public void unsetMsg() {
    this.msg = null;
  }

  // Returns true if field msg is set (has been asigned a value) and false otherwise
  public boolean isSetMsg() {
    return this.msg != null;
  }

  public void setMsgIsSet(boolean value) {
    if (!value) {
      this.msg = null;
    }
  }

  public void setFieldValue(int fieldID, Object value) {
    switch (fieldID) {
    case MSG:
      if (value == null) {
        unsetMsg();
      } else {
        setMsg((String)value);
      }
      break;

    default:
      throw new IllegalArgumentException("Field " + fieldID + " doesn't exist!");
    }
  }

  public Object getFieldValue(int fieldID) {
    switch (fieldID) {
    case MSG:
      return getMsg();

    default:
      throw new IllegalArgumentException("Field " + fieldID + " doesn't exist!");
    }
  }

  // Returns true if field corresponding to fieldID is set (has been asigned a value) and false otherwise
  public boolean isSet(int fieldID) {
    switch (fieldID) {
    case MSG:
      return isSetMsg();
    default:
      throw new IllegalArgumentException("Field " + fieldID + " doesn't exist!");
    }
  }

  @Override
  public boolean equals(Object that) {
    if (that == null)
      return false;
    if (that instanceof ConstructionError)
      return this.equals((ConstructionError)that);
    return false;
  }

  public boolean equals(ConstructionError that) {
    if (that == null)
      return false;

    boolean this_present_msg = true && this.isSetMsg();
    boolean that_present_msg = true && that.isSetMsg();
    if (this_present_msg || that_present_msg) {
      if (!(this_present_msg && that_present_msg))
        return false;
      if (!this.msg.equals(that.msg))
        return false;
    }

    return true;
  }

  @Override
  public int hashCode() {
    return 0;
  }

  public int compareTo(ConstructionError other) {
    if (!getClass().equals(other.getClass())) {
      return getClass().getName().compareTo(other.getClass().getName());
    }

    int lastComparison = 0;
    ConstructionError typedOther = (ConstructionError)other;

    lastComparison = Boolean.valueOf(isSetMsg()).compareTo(isSetMsg());
    if (lastComparison != 0) {
      return lastComparison;
    }
    lastComparison = TBaseHelper.compareTo(msg, typedOther.msg);
    if (lastComparison != 0) {
      return lastComparison;
    }
    return 0;
  }

  public void read(TProtocol iprot) throws TException {
    TField field;
    iprot.readStructBegin();
    while (true)
    {
      field = iprot.readFieldBegin();
      if (field.type == TType.STOP) { 
        break;
      }
      switch (field.id)
      {
        case MSG:
          if (field.type == TType.STRING) {
            this.msg = iprot.readString();
          } else { 
            TProtocolUtil.skip(iprot, field.type);
          }
          break;
        default:
          TProtocolUtil.skip(iprot, field.type);
          break;
      }
      iprot.readFieldEnd();
    }
    iprot.readStructEnd();


    // check for required fields of primitive type, which can't be checked in the validate method
    validate();
  }

  public void write(TProtocol oprot) throws TException {
    validate();

    oprot.writeStructBegin(STRUCT_DESC);
    if (this.msg != null) {
      oprot.writeFieldBegin(MSG_FIELD_DESC);
      oprot.writeString(this.msg);
      oprot.writeFieldEnd();
    }
    oprot.writeFieldStop();
    oprot.writeStructEnd();
  }

  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder("ConstructionError(");
    boolean first = true;

    sb.append("msg:");
    if (this.msg == null) {
      sb.append("null");
    } else {
      sb.append(this.msg);
    }
    first = false;
    sb.append(")");
    return sb.toString();
  }

  public void validate() throws TException {
    // check for required fields
    // check that fields of type enum have valid values
  }

}
