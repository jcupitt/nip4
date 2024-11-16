<?xml version="1.0"?>
<root xmlns="http://www.vips.ecs.soton.ac.uk/nip/9.0.0">
  <Workspace view="WORKSPACE_MODE_REGULAR" scale="1" offset="0" locked="false" lpane_position="200" lpane_open="false" rpane_position="400" rpane_open="false" local_defs="// private definitions for this tab&#10;" name="tab1" filename="$HOME/GIT/nip4/test/workspaces/test.ws" major="9" minor="0">
    <Column x="5" y="5" open="true" selected="true" sform="false" next="5" name="A">
      <Subcolumn vislevel="3">
        <Row popup="false" name="A1">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="Image_file &quot;/home/john/pics/k2.jpg&quot;"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A2">
          <Rhs vislevel="4" flags="7">
            <Slider caption="hello" from="0" to="255" value="2.5499999999999998"/>
            <Subcolumn vislevel="2"/>
            <iText formula="Scale &quot;hello&quot; 0 255 128"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A3">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="A1 * A2"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A4">
          <Rhs vislevel="1" flags="4">
            <iText formula="[1..100]"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
  </Workspace>
</root>
