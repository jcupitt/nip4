<?xml version="1.0"?>
<root xmlns="http://www.vips.ecs.soton.ac.uk/nip/9.0.0">
  <Workspace view="WORKSPACE_MODE_REGULAR" scale="1" offset="0" locked="false" lpane_position="200" lpane_open="false" rpane_position="400" rpane_open="false" local_defs="// private definitions for this tab&#10;" name="tab2" filename="$HOME/GIT/nip4/test/workspaces/x.ws" major="9" minor="0">
    <Column x="5" y="5" open="true" selected="false" sform="false" next="10" name="A">
      <Subcolumn vislevel="3">
        <Row popup="false" name="A1">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="Image_file &quot;$HOME/GIT/nip4/share/nip4/data/examples/print_test_image.v&quot;"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A2">
          <Rhs vislevel="1" flags="1">
            <iRegion image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true" left="0" top="0" width="545" height="366">
              <iRegiongroup/>
            </iRegion>
            <Subcolumn vislevel="0"/>
            <iText formula="Region A1 29 75 272 241"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A3">
          <Rhs vislevel="1" flags="1">
            <Expression caption="Patches across chart"/>
            <Subcolumn vislevel="0"/>
            <iText formula="Expression &quot;Patches across chart&quot; 6"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A4">
          <Rhs vislevel="1" flags="1">
            <Expression caption="Patches down chart"/>
            <Subcolumn vislevel="0"/>
            <iText formula="Expression &quot;Patches down chart&quot; 6"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A5">
          <Rhs vislevel="1" flags="1">
            <Slider/>
            <Subcolumn vislevel="0"/>
            <iText formula="Scale &quot;Measure area (%)&quot; 1 100 50"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A6">
          <Rhs vislevel="1" flags="4">
            <iText formula="measure_draw (to_real A3) (to_real A4) (to_real A5) A2"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
    <Column x="429" y="5" open="true" selected="true" sform="false" next="5" name="B">
      <Subcolumn vislevel="3">
        <Row popup="false" name="B1">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="Image_file &quot;$HOME/GIT/nip4/share/nip4/data/examples/print_test_image.v&quot;"/>
          </Rhs>
        </Row>
        <Row popup="false" name="B2">
          <Rhs vislevel="1" flags="1">
            <iRegion image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true" left="0" top="0" width="545" height="366">
              <iRegiongroup/>
            </iRegion>
            <Subcolumn vislevel="0"/>
            <iText formula="Region B1 29 75 272 241"/>
          </Rhs>
        </Row>
        <Row popup="false" name="patch_width">
          <Rhs vislevel="1" flags="4">
            <iText formula="B2.width / 6"/>
          </Rhs>
        </Row>
        <Row popup="false" name="sample_width">
          <Rhs vislevel="1" flags="4">
            <iText formula="patch_width * (50 / 100)"/>
          </Rhs>
        </Row>
        <Row popup="false" name="left_margin">
          <Rhs vislevel="1" flags="4">
            <iText formula="(patch_width - sample_width) / 2"/>
          </Rhs>
        </Row>
        <Row popup="false" name="patch_height">
          <Rhs vislevel="1" flags="4">
            <iText formula="B2.height / 4"/>
          </Rhs>
        </Row>
        <Row popup="false" name="sample_height">
          <Rhs vislevel="1" flags="4">
            <iText formula="patch_height * (50 / 100)"/>
          </Rhs>
        </Row>
        <Row popup="false" name="top_margin">
          <Rhs vislevel="1" flags="4">
            <iText formula="(patch_height - sample_height) / 2"/>
          </Rhs>
        </Row>
        <Row popup="false" name="cods">
          <Rhs vislevel="1" flags="4">
            <iText formula="[[x * patch_width + left_margin, y * patch_height + top_margin] ::    y &lt;- [0 .. 4 - 1]; x &lt;- [0 .. 6 - 1]]"/>
          </Rhs>
        </Row>
        <Row popup="false" name="x">
          <Rhs vislevel="1" flags="4">
            <iText formula="map (extract 0) cods"/>
          </Rhs>
        </Row>
        <Row popup="false" name="y">
          <Rhs vislevel="1" flags="4">
            <iText formula="map (extract 1) cods"/>
          </Rhs>
        </Row>
        <Row popup="false" name="outer">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="mkim [$pixel =&gt; 255] sample_width sample_height 1"/>
          </Rhs>
        </Row>
        <Row popup="false" name="inner">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="mkim [] (sample_width - 4) (sample_height - 4) 1"/>
          </Rhs>
        </Row>
        <Row popup="false" name="patch">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="insert 2 2 inner outer"/>
          </Rhs>
        </Row>
        <Row popup="false" name="bg">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="mkim [] B2.width B2.height 1"/>
          </Rhs>
        </Row>
        <Row popup="false" name="B4">
          <Rhs vislevel="1" flags="4">
            <iText formula="im_insertset bg.value patch.value x y"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
  </Workspace>
</root>
