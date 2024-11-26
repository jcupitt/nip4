<?xml version="1.0"?>
<root xmlns="http://www.vips.ecs.soton.ac.uk/nip/9.0.0">
  <Workspace view="WORKSPACE_MODE_REGULAR" scale="1" offset="0" locked="false" lpane_position="200" lpane_open="false" rpane_position="400" rpane_open="false" local_defs="// private definitions for this tab&#10;" name="tab2" filename="$HOME/GIT/nip4/test/workspaces/test.ws" major="9" minor="0">
    <Column x="5" y="5" open="true" selected="false" sform="false" next="7" name="A">
      <Subcolumn vislevel="3">
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
        <Row popup="false" name="A5">
          <Rhs vislevel="1" flags="1">
            <iArrow>
              <iRegiongroup/>
            </iArrow>
            <Subcolumn vislevel="0"/>
            <iText formula="Arrow A1 976 962 (-586) (-634)"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A6">
          <Rhs vislevel="3" flags="7">
            <Plot plot_left="0" plot_top="0" plot_mag="100" show_status="false"/>
            <Subcolumn vislevel="1"/>
            <iText formula="Hist_graph_item.action A5"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
    <Column x="552" y="5" open="true" selected="true" sform="false" next="2" name="B">
      <Subcolumn vislevel="3">
        <Row popup="false" name="A1">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="Image_file &quot;$HOME/pics/k2.jpg&quot;"/>
          </Rhs>
        </Row>
        <Row popup="false" name="B1">
          <Rhs vislevel="1" flags="1">
            <iRegion image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true">
              <iRegiongroup/>
            </iRegion>
            <Subcolumn vislevel="0"/>
            <iText formula="Region A1 1040 1461 43 84"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
  </Workspace>
</root>
